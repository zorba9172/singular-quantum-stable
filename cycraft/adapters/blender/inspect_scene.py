"""Reopen a published scene with native importers and report observed contents."""
import bpy
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from scene_io import census, validate_asset, file_digest
from animation import inspect_rig_motion, inspect_shape_key_motion
from material_observation import inspect_materials
from interchange import restore_render_state, render_state

args = sys.argv[sys.argv.index("--") + 1:]
if len(args) != 2:
    raise SystemExit("Expected SCENE_FILE NEW_REPORT.json")
source, report = (Path(arg).resolve() for arg in args)
if report.exists() or not source.is_file():
    raise ValueError("Inspection requires an existing source and a new report path")
bpy.ops.wm.read_factory_settings(use_empty=True)
restored = {"status": "NOT_REQUIRED_NATIVE_BLEND"}
if source.suffix == ".blend":
    bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
elif source.suffix in (".glb", ".gltf"):
    validate_asset(source)
    bpy.ops.import_scene.gltf(filepath=str(source))
    restored = restore_render_state(source, file_digest)
elif source.suffix in (".usd", ".usda", ".usdc"):
    bpy.ops.wm.usd_import(filepath=str(source))
    restored = {"status": "NOT_APPLIED_USD_NATIVE_IMPORT"}
else:
    raise ValueError("Unsupported scene format")
inventory = census()
if inventory["mesh_objects"] == 0 or inventory["instanced_triangles"] == 0:
    raise ValueError("Imported scene has no renderable mesh geometry")
scene = bpy.context.scene
result = {
    "status": "IMPORTED_WITH_GEOMETRY",
    "source": str(source), "sha256": file_digest(source),
    "blender_version": bpy.app.version_string,
    "inventory": inventory,
    "camera_names": sorted(o.name for o in scene.objects if o.type == "CAMERA"),
    "light_types": sorted(o.data.type for o in scene.objects if o.type == "LIGHT"),
    "frame_range": [scene.frame_start, scene.frame_end],
    "armatures": [{"object": obj.name, "bones": len(obj.data.bones),
                   "deform_bones": sum(bone.use_deform for bone in obj.data.bones)}
                  for obj in scene.objects if obj.type == "ARMATURE"],
    "shape_key_blocks": sum(len(mesh.shape_keys.key_blocks) for mesh in bpy.data.meshes
                            if mesh.shape_keys),
    "actions": len(bpy.data.actions),
    "linked_libraries": [library.filepath for library in bpy.data.libraries],
    "packed_file_images": sum(bool(image.packed_file) for image in bpy.data.images
                              if image.source == "FILE" and image.users),
    "appended_packages": [{"object": o.name, "source": o["source_uri"],
        "sha256": o["source_sha256"], "collection": o["source_collection"],
        "actions": json.loads(o.get("source_actions", "[]"))}
        for o in scene.objects if "source_uri" in o],
    "collections": [{"name": c.name, "hide_render": c.hide_render,
                     "objects": len(c.all_objects)} for c in bpy.data.collections],
    "mesh_names": [o.name for o in scene.objects if o.type == "MESH"],
    "embedded_text_blocks": len(bpy.data.texts),
    "autoexec_enabled": bpy.context.preferences.filepaths.use_scripts_auto_execute,
    "render_equivalence_verified": False,
    "rig_motion": inspect_rig_motion(),
    "shape_key_motion": inspect_shape_key_motion(),
    "interchange": restored,
    "render_state": render_state(),
    "material_observations": inspect_materials(),
}
report.parent.mkdir(parents=True, exist_ok=True)
with report.open("x") as stream:
    json.dump(result, stream, indent=2)
print("SCENE_INSPECTED", json.dumps(result), flush=True)
