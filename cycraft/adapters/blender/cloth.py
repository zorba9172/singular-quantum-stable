"""Execute bounded native-authored cloth; export owns sequential simulation capture."""
import json
import math
import bpy
from mathutils.bvhtree import BVHTree

def bounded(value, name, low, high, integer=False):
    kinds = (int,) if integer else (int, float)
    if type(value) not in kinds or not math.isfinite(value) or not low <= value <= high:
        raise ValueError(f"Cloth {name} outside [{low}, {high}]")
    return value

def mesh_descendants(root):
    pending, meshes, visited = [root], [], set()
    while pending:
        obj = pending.pop()
        if obj.name in visited:
            continue
        visited.add(obj.name)
        if len(visited) > 256:
            raise ValueError("Cloth collection descendant budget exceeded")
        if obj.type == "MESH":
            meshes.append(obj)
        pending.extend(obj.children)
    return sorted(meshes, key=lambda obj: obj.name)

def collection_overrides(root, spec, materials):
    meshes = mesh_descendants(root)
    if "material_override" in spec:
        material = materials[spec["material_override"]]
        for obj in meshes:
            obj.data.materials.clear()
            obj.data.materials.append(material)
            for polygon in obj.data.polygons:
                polygon.material_index = 0
    return meshes

def fit_surface_pins(obj, pins, surface, objects, references):
    """Fit explicit pins to an evaluated surface at the assembly frame only."""
    if not isinstance(surface, dict) or set(surface) != {"object", "max_distance_m", "offset_m"}:
        raise ValueError("Invalid native cloth pin_surface contract")
    reference = surface["object"]
    if reference not in references:
        raise ValueError("Pin surface must be an explicit cloth collision target")
    limit = bounded(surface["max_distance_m"], "pin fitting distance", 0.001, 1)
    offset = bounded(surface["offset_m"], "pin surface offset", 0, 0.1)
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    originals = [obj.matrix_world @ obj.data.vertices[index].co for index in pins]
    nearest = [None] * len(pins)
    vertices_used, loops_used = 0, 0
    for target in mesh_descendants(objects[reference]):
        evaluated = target.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        tree = None
        try:
            vertices_used += len(mesh.vertices)
            loops_used += len(mesh.loops)
            if vertices_used > 2000000 or loops_used > 8000000:
                raise ValueError("Pin fitting evaluated geometry budget exceeded")
            matrix = evaluated.matrix_world
            if abs(matrix.determinant()) < 1e-12:
                raise ValueError("Pin fitting target has singular transform")
            world = [matrix @ vertex.co for vertex in mesh.vertices]
            if any(not math.isfinite(value) for point in world for value in point):
                raise ValueError("Pin fitting surface is nonfinite")
            faces = [tuple(polygon.vertices) for polygon in mesh.polygons]
            if matrix.determinant() < 0:
                faces = [face[::-1] for face in faces]
            tree = BVHTree.FromPolygons(world, faces, all_triangles=False)
            del world, faces
            for index, point in enumerate(originals):
                hit, normal, face, distance = tree.find_nearest(point, limit)
                if hit is not None and (nearest[index] is None or distance < nearest[index][0]):
                    nearest[index] = (distance, hit.copy(), normal.copy(), target.name)
        finally:
            tree = None
            evaluated.to_mesh_clear()
    if any(hit is None for hit in nearest):
        raise ValueError("Authored cloth pin exceeds native surface fitting distance")
    if abs(obj.matrix_world.determinant()) < 1e-12:
        raise ValueError("Cloth pin fitting object has singular transform")
    inverse, evidence = obj.matrix_world.inverted(), []
    for index, original, hit in zip(pins, originals, nearest):
        distance, point, normal, target = hit
        if normal.length < 1e-8:
            raise ValueError("Cloth pin surface has no valid outward normal")
        normal.normalize()
        anchor = point + normal * offset
        obj.data.vertices[index].co = inverse @ anchor
        evidence.append({"vertex": index, "original_world_m": list(original),
                         "surface_world_m": list(point), "anchor_world_m": list(anchor),
                         "surface_normal_world": list(normal), "nearest_distance_m": distance,
                         "target": target, "offset_m": offset})
    obj.data.update()
    return {"scope": "Static surface fit at assembly frame; no moving-body attachment",
            "frame": bpy.context.scene.frame_current, "target_root": reference,
            "evaluated_vertices": vertices_used, "pins": evidence}

def configure_cloth(scene, contract, objects):
    authored = [spec for spec in contract["objects"] if "cloth" in spec]
    if not authored:
        return {"configured": False, "objects": []}
    frames = scene.frame_end - scene.frame_start + 1
    if len(authored) > 8 or frames > 240:
        raise ValueError("Cloth object/frame budget exceeded")
    gravity = contract["physics"]["gravity_m_s2"]
    if len(gravity) != 3:
        raise ValueError("Cloth gravity must be a native metric vector")
    scene.gravity = [bounded(v, "gravity", -100, 100) for v in gravity]
    scene.use_gravity = True
    source = {spec["id"]: spec for spec in contract["objects"]}
    if len(source) != len(contract["objects"]):
        raise ValueError("Duplicate native object identifiers")
    total, report = 0, []
    for spec in authored:
        obj, cfg = objects[spec["id"]], spec["cloth"]
        if spec["kind"] != "mesh" or spec.get("topology") != "open_surface" or obj.modifiers:
            raise ValueError("Cloth requires an unmodified native open mesh")
        if cfg["model"] != "blender.cloth/1":
            raise ValueError("Unsupported cloth model")
        total += len(obj.data.vertices) * frames
        if total > 1000000:
            raise ValueError("Cloth vertex-frame budget exceeded")
        pins = cfg["pins"]
        if not isinstance(pins, list) or not 1 <= len(pins) < len(obj.data.vertices):
            raise ValueError("Cloth must have pins and free vertices")
        if any(type(i) is not int or not 0 <= i < len(obj.data.vertices) for i in pins) or len(set(pins)) != len(pins):
            raise ValueError("Invalid cloth pin indices")
        references = cfg["collision_objects"]
        if not isinstance(references, list) or not 1 <= len(references) <= 32 or len(set(references)) != len(references):
            raise ValueError("Cloth collider reference budget exceeded")
        fitting = None
        if "pin_surface" in cfg:
            fitting = fit_surface_pins(obj, pins, cfg["pin_surface"], objects, references)
        distance = bounded(cfg["collision_distance_m"], "collision distance", 0.001, 0.1)
        colliders = {}
        for reference in references:
            if source[reference].get("collision") is not True or reference == spec["id"]:
                raise ValueError("Cloth collider must be explicitly enabled")
            for target in mesh_descendants(objects[reference]):
                colliders[target.name] = target
        if not 1 <= len(colliders) <= 256:
            raise ValueError("Cloth collider mesh budget exceeded")
        collection = bpy.data.collections.new(f"{obj.name} collision targets")
        scene.collection.children.link(collection)
        for target in sorted(colliders.values(), key=lambda item: item.name):
            collection.objects.link(target)
            if not any(mod.type == "COLLISION" for mod in target.modifiers):
                target.modifiers.new("Native cloth collision", "COLLISION")
            if "singular_collision_distance_m" in target and abs(target["singular_collision_distance_m"] - distance) > 1e-9:
                raise ValueError("Shared cloth collider has conflicting native distances")
            target.collision.thickness_outer = distance
            target["singular_collision_distance_m"] = distance
        group = obj.vertex_groups.new(name="Native cloth pins")
        group.add(pins, 1.0, "REPLACE")
        modifier = obj.modifiers.new("Native cloth simulation", "CLOTH")
        settings = modifier.settings
        settings.mass = bounded(cfg["mass_kg_per_vertex"], "mass", 0.000001, 10)
        settings.quality = bounded(cfg["quality_steps"], "quality", 1, 20, True)
        for field in ("tension_stiffness", "compression_stiffness", "shear_stiffness", "bending_stiffness",
                      "tension_damping", "compression_damping", "shear_damping", "bending_damping", "air_damping"):
            setattr(settings, field, bounded(cfg[field], field, 0, 50 if "damping" in field else 10000))
        settings.vertex_group_mass = group.name
        collision = modifier.collision_settings
        collision.use_collision = True
        collision.collection = collection
        collision.distance_min = distance
        collision.collision_quality = bounded(cfg["collision_quality_steps"], "collision quality", 1, 20, True)
        if type(cfg["self_collision"]) is not bool:
            raise ValueError("Cloth self_collision must be boolean")
        collision.use_self_collision = cfg["self_collision"]
        collision.self_distance_min = bounded(cfg["self_collision_distance_m"], "self distance", 0.001, 0.1)
        modifier.point_cache.frame_start = scene.frame_start
        modifier.point_cache.frame_end = scene.frame_end
        obj["singular_cloth_pins"] = json.dumps(pins)
        report.append({"object": obj.name, "vertices": len(obj.data.vertices), "pins": pins,
                       "collision_meshes": sorted(colliders), "native_settings": cfg,
                       "pin_surface_fitting": fitting})
    return {"configured": True, "engine": "Blender Cloth", "gravity_m_s2": list(scene.gravity),
            "vertex_frames": total, "objects": report, "deformation_evidence": "deformation.json"}
