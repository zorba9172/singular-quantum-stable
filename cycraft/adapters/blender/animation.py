"""Apply bounded, native-authored rig tracks through Blender's data API."""
import bpy
import hashlib
import json
import math
import struct
from array import array

def sampled_frames(scene):
    if not 0 <= scene.frame_end - scene.frame_start < 240:
        raise ValueError("Motion inspection frame budget exceeded")
    return sorted({scene.frame_start, (scene.frame_start + scene.frame_end)//2, scene.frame_end})

def sample_mesh(obj, graph, remaining, inverse=None):
    """Shared evaluated-coordinate read; caller owns aggregate sampling budget."""
    evaluated = obj.evaluated_get(graph)
    mesh = evaluated.to_mesh()
    try:
        count = len(mesh.vertices)
        if not 0 < count <= remaining:
            raise ValueError("Motion inspection vertex budget exceeded")
        transform = evaluated.matrix_world if inverse is None else inverse @ evaluated.matrix_world
        coordinates = array("f")
        digest, local = hashlib.sha256(), hashlib.sha256()
        low, high = [math.inf]*3, [-math.inf]*3
        for vertex in mesh.vertices:
            point = transform @ vertex.co
            if any(not math.isfinite(v) for v in (*point, *vertex.co)):
                raise ValueError("Nonfinite inspected mesh coordinate")
            coordinates.extend(point)
            digest.update(struct.pack("<3f", *point))
            local.update(struct.pack("<3f", *vertex.co))
            for axis in range(3):
                low[axis], high[axis] = min(low[axis], point[axis]), max(high[axis], point[axis])
        return coordinates, {"evaluated_vertices": count, "evaluated_polygons": len(mesh.polygons),
            "geometry_sha256": digest.hexdigest(), "local_geometry_sha256": local.hexdigest(),
            "bounds": {"min": low, "max": high}}
    finally:
        evaluated.to_mesh_clear()

def apply_rig_animation(collection, spec):
    animation = spec.get("rig_animation")
    if animation is None:
        return
    rig = collection.all_objects.get(animation["armature"])
    if rig is None or rig.type != "ARMATURE":
        raise ValueError("Animation armature is missing from appended collection")
    properties, tracks = animation.get("properties", []), animation["tracks"]
    if len(properties) > 128 or not 1 <= len(tracks) <= 128:
        raise ValueError("Rig animation budget exceeded")
    scene = bpy.context.scene
    for setting in properties:
        bone = rig.pose.bones.get(setting["bone"])
        value = setting["value"]
        if bone is None or setting["name"] not in bone:
            raise ValueError("Rig property is missing")
        if type(value) not in (float, int) or not math.isfinite(value):
            raise ValueError("Rig property must be a finite number")
        bone[setting["name"]] = value
    seen = set()
    for track in tracks:
        bone = rig.pose.bones.get(track["bone"])
        if bone is None or bone.name in seen:
            raise ValueError("Rig track bone is missing or duplicated")
        seen.add(bone.name)
        keys = track["keyframes"]
        if not 1 <= len(keys) <= 240:
            raise ValueError("Rig keyframe budget exceeded")
        bone.rotation_mode = "XYZ"
        previous = scene.frame_start - 1
        for key in keys:
            frame, rotation = key["frame"], key["rotation_degrees"]
            if type(frame) is not int or not previous < frame <= scene.frame_end:
                raise ValueError("Rig keys must increase within the scene frame range")
            if len(rotation) != 3 or any(type(v) not in (int, float) or
                    not math.isfinite(v) or abs(v) > 360 for v in rotation):
                raise ValueError("Rig rotation must contain three finite bounded angles")
            bone.rotation_euler = tuple(math.radians(v) for v in rotation)
            bone.keyframe_insert(data_path="rotation_euler", frame=frame)
            previous = frame
    rig["native_animation_tracks"] = json.dumps(sorted(seen))
    rig.update_tag()

def inspect_rig_motion():
    """Measure evaluated skinned geometry in rig space, excluding root/camera motion."""
    scene = bpy.context.scene
    saved_frame = scene.frame_current
    results = []
    try:
        for rig in scene.objects:
            if rig.type != "ARMATURE" or "native_animation_tracks" not in rig:
                continue
            names = json.loads(rig["native_animation_tracks"])
            meshes = sorted((o for o in scene.objects if o.type == "MESH" and any(
                m.type == "ARMATURE" and m.object == rig for m in o.modifiers)),
                key=lambda o: o.name)
            samples = []
            for frame in sampled_frames(scene):
                scene.frame_set(frame)
                bpy.context.view_layer.update()
                graph = bpy.context.evaluated_depsgraph_get()
                digest, count = hashlib.sha256(), 0
                inverse = rig.matrix_world.inverted()
                for obj in meshes:
                    coordinates, sample = sample_mesh(obj, graph, 2000000-count, inverse)
                    count += sample["evaluated_vertices"]
                    digest.update(obj.name.encode())
                    # Explicit little-endian encoding preserves the existing rig hash contract.
                    for index in range(0, len(coordinates), 3):
                        digest.update(struct.pack("<3f", *coordinates[index:index+3]))
                samples.append({"frame": frame, "evaluated_vertices": count,
                    "rig_space_geometry_sha256": digest.hexdigest(),
                    "bone_heads": {name: list(rig.pose.bones[name].head) for name in names}})
            results.append({"armature": rig.name, "skinned_meshes": len(meshes),
                            "samples": samples})
    finally:
        scene.frame_set(saved_frame)
    return results


def inspect_shape_key_motion():
    """Observe serialized morph geometry; this is not a simulation-quality verdict."""
    scene = bpy.context.scene
    if len(scene.objects) > 10000:
        raise ValueError("Motion inspection object budget exceeded")
    objects = sorted((o for o in scene.objects if o.type == "MESH" and o.data.shape_keys),
                     key=lambda o: o.name)
    if len(objects) > 32:
        raise ValueError("Shape-key inspection object budget exceeded")
    result = {"status": "MEASURED" if objects else "NO_SHAPE_KEY_MESHES",
        "coordinate_space": "WORLD", "simulation_quality_verified": False,
        "limitations": ["Three integer-frame observations, not continuous-motion validation",
            "World displacement includes object transforms; local hashes identify mesh-coordinate changes",
            "Correspondence uses stable per-object vertex order, not cross-format vertex indices"],
        "objects": []}
    saved_frame, saved_subframe = scene.frame_current, scene.frame_subframe
    baseline, total = {}, 0
    items = {o.name: {"object": o.name, "shape_key_blocks": len(o.data.shape_keys.key_blocks),
        "samples": []} for o in objects}
    try:
        for frame in sampled_frames(scene):
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            graph = bpy.context.evaluated_depsgraph_get()
            for obj in objects:
                coordinates, sample = sample_mesh(obj, graph, min(2000000, 6000000-total))
                total += sample["evaluated_vertices"]
                if obj.name not in baseline:
                    baseline[obj.name] = coordinates
                first = baseline[obj.name]
                if len(first) != len(coordinates):
                    raise ValueError("Shape-key inspected vertex count changed")
                displacement = max(math.sqrt(sum((coordinates[i+j]-first[i+j])**2
                    for j in range(3))) for i in range(0, len(first), 3))
                sample.update({"frame": frame, "max_displacement_from_start_m": displacement})
                items[obj.name]["samples"].append(sample)
        result["objects"] = list(items.values())
        result["sampled_vertex_frames"] = total
        return result
    finally:
        scene.frame_set(saved_frame, subframe=saved_subframe)
