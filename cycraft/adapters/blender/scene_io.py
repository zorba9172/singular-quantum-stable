"""Strict asset import and scene serialization through Blender's native operators."""
import bpy
import json
import math
import hashlib
from pathlib import Path
from animation import apply_rig_animation
from geometry import normalize_prototype_meshes
from asset_material_overrides import apply_asset_material_overrides
from deformation import bake_deformations
from interchange import export_render_state

def file_digest(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()

def append_collection(spec):
    path = Path(spec["uri"]).resolve()
    if path.suffix != ".blend" or not path.is_file():
        raise ValueError("Collection source must be an existing Blender file")
    before_images = set(bpy.data.images)
    with bpy.data.libraries.load(str(path), link=False) as (source, target):
        if spec["collection"] not in source.collections:
            raise ValueError(f"Collection missing: {spec['collection']}")
        target.collections = [spec["collection"]]
        if spec.get("preserve_actions", False):
            if len(source.actions) > 256:
                raise ValueError("Asset action library budget exceeded")
            target.actions = source.actions
    collection = target.collections[0]
    bpy.context.scene.collection.children.link(collection)
    root = bpy.data.objects.new(spec["id"], None)
    root["source_uri"] = str(path)
    root["source_sha256"] = file_digest(path)
    root["source_collection"] = spec["collection"]
    if spec.get("preserve_actions", False):
        for action in target.actions:
            action.use_fake_user = True
        root["source_actions"] = json.dumps([action.name for action in target.actions])
    bpy.context.scene.collection.objects.link(root)
    members = set(collection.all_objects)
    for obj in members:
        if obj.parent not in members:
            transform = obj.matrix_world.copy()
            obj.parent = root
            obj.matrix_world = transform
    root.location = spec["location"]
    root.rotation_euler.z = math.radians(spec["yaw_degrees"])
    root.scale = spec["scale"]
    for image in set(bpy.data.images) - before_images:
        if image.source != "FILE" or image.packed_file:
            continue
        actual = Path(bpy.path.abspath(image.filepath))
        if not actual.is_file() and image.filepath.startswith("//"):
            actual = path.parent / image.filepath[2:]
        if not actual.is_file():
            raise ValueError(f"Appended collection texture missing: {image.filepath}")
        image.filepath = str(actual.resolve())
        image.pack()
    apply_rig_animation(collection, spec)
    return root

def validate_asset(path):
    path = Path(path).resolve()
    if not path.is_file():
        raise ValueError(f"Missing asset: {path}")
    if path.suffix == ".gltf":
        data = json.loads(path.read_text())
        for record in data.get("buffers", []) + data.get("images", []):
            uri = record.get("uri", "")
            if uri and not uri.startswith("data:"):
                target = (path.parent / uri).resolve()
                if not target.is_relative_to(path.parent) or not target.is_file():
                    raise ValueError(f"Missing or out-of-package resource: {uri}")
    elif path.suffix == ".glb":
        import struct
        with path.open("rb") as stream:
            header = stream.read(12)
        if len(header) != 12:
            raise ValueError("Truncated GLB")
        magic, version, length = struct.unpack("<4sII", header)
        if magic != b"glTF" or version != 2 or length != path.stat().st_size:
            raise ValueError("Invalid GLB header")
    else:
        raise ValueError(f"Unsupported asset type: {path.suffix}")
    return path

def import_prototype(spec):
    path = validate_asset(spec["uri"])
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    imported = set(bpy.data.objects) - before
    meshes = [obj for obj in imported if obj.type == "MESH"]
    selected = set(spec.get("mesh_names", []))
    if selected:
        missing = selected - {obj.name for obj in meshes}
        if missing:
            raise ValueError(f"Asset selection names are missing: {sorted(missing)}")
        meshes = [obj for obj in meshes if obj.name in selected]
    if not meshes:
        raise ValueError(f"Asset has no mesh: {path}")
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    result = []
    try:
        for obj in meshes:
            evaluated = obj.evaluated_get(graph)
            mesh = bpy.data.meshes.new_from_object(evaluated,
                preserve_all_data_layers=True, depsgraph=graph)
            result.append(mesh)
            mesh.name = spec["id"] + "_" + obj.data.name
            mesh.transform(evaluated.matrix_world)
        normalize_prototype_meshes(result, spec["normalize_height"])
        apply_asset_material_overrides(result, spec, path)
    except Exception:
        for mesh in result:
            bpy.data.meshes.remove(mesh)
        raise
    finally:
        for obj in imported:
            bpy.data.objects.remove(obj, do_unlink=True)
    return result

def census():
    depsgraph = bpy.context.evaluated_depsgraph_get()
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    triangles = 0
    for obj in meshes:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        try:
            mesh.calc_loop_triangles()
            triangles += len(mesh.loop_triangles)
        finally:
            evaluated.to_mesh_clear()
    images = [im for im in bpy.data.images if im.source == "FILE" and im.users]
    missing = []
    for image in images:
        if not image.packed_file and not Path(bpy.path.abspath(image.filepath)).is_file():
            missing.append(image.filepath)
    if missing:
        raise ValueError(f"Missing textures: {missing}")
    return {"mesh_objects": len(meshes), "instanced_triangles": triangles,
            "materials": len([m for m in bpy.data.materials if m.users]),
            "images": len(images),
            "cameras": len([o for o in bpy.context.scene.objects if o.type == "CAMERA"]),
            "lights": len([o for o in bpy.context.scene.objects if o.type == "LIGHT"])}

def export_gltf(output):
    # Applying all modifiers removes shape keys while animation channels can
    # still reference them. Bake only static meshes and keep rig data intact.
    graph = bpy.context.evaluated_depsgraph_get()
    baked = []
    vertices = 0
    try:
        for obj in bpy.context.scene.objects:
            if obj.type != "MESH" or not obj.modifiers or obj.data.shape_keys or any(
                    modifier.type == "ARMATURE" for modifier in obj.modifiers):
                continue
            mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(graph),
                preserve_all_data_layers=True, depsgraph=graph)
            flags = [(m, m.show_viewport, m.show_render) for m in obj.modifiers]
            baked.append((obj, obj.data, mesh, flags))
            vertices += len(mesh.vertices)
            if vertices > 2000000:
                raise ValueError("Static export geometry budget exceeded")
            obj.data = mesh
            for modifier, _, _ in flags:
                modifier.show_viewport = modifier.show_render = False
        bpy.ops.export_scene.gltf(filepath=str(output / "nocturne.glb"), export_format="GLB",
            export_apply=False, export_cameras=True, export_lights=True,
            export_animation_mode="SCENE", export_frame_range=True,
            export_force_sampling=True, export_morph=True, export_morph_animation=True)
    finally:
        for obj, original, mesh, flags in reversed(baked):
            obj.data = original
            for modifier, viewport, render in flags:
                modifier.show_viewport, modifier.show_render = viewport, render
            bpy.data.meshes.remove(mesh)

def export_scene(output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    for image in bpy.data.images:
        if image.source == "FILE" and image.users and not image.packed_file:
            image.pack()
    with bake_deformations(bpy.context.scene) as deformation:
        bpy.ops.wm.save_as_mainfile(filepath=str(output / "nocturne.blend"))
        export_gltf(output)
        bpy.ops.wm.usd_export(filepath=str(output / "nocturne.usdc"),
            export_materials=True, export_textures_mode="NEW", selected_objects_only=False,
            export_animation=True, use_instancing=True)
        export_render_state(output, file_digest)
        (output / "deformation.json").write_text(json.dumps(deformation, indent=2))
    return census()

