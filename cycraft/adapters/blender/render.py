"""Render an already serialized scene. No scene-generation logic lives here."""
import bpy
import json
import sys
import time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from scene_io import file_digest

args = sys.argv[sys.argv.index("--")+1:]
if len(args) not in (4, 5, 6):
    raise SystemExit("Expected OUTPUT_DIRECTORY FRAME CAMERA CPU|METAL [LAST_FRAME [FINAL|PREVIEW]]")
output, frame, camera, device = Path(args[0]), int(args[1]), args[2], args[3]
last_frame = int(args[4]) if len(args) >= 5 else frame
profile = args[5] if len(args) == 6 else "FINAL"
if profile not in ("FINAL", "PREVIEW"):
    raise ValueError("Unsupported render profile")
if not 1 <= frame <= last_frame <= 999999 or last_frame - frame >= 240:
    raise ValueError("Invalid bounded frame range")
scene = bpy.context.scene
if scene.render.resolution_percentage != 100:
    raise ValueError("Production renders require 100 percent native resolution")
if scene.render.use_border and scene.render.use_crop_to_border:
    raise ValueError("Production renders cannot crop the declared frame dimensions")
if not 1 <= scene.render.resolution_x <= 8192 or not 1 <= scene.render.resolution_y <= 8192:
    raise ValueError("Production frame dimensions exceed budget")
source_dimensions = [scene.render.resolution_x, scene.render.resolution_y]
if profile == "PREVIEW":
    scale = min(1.0, 960.0 / max(source_dimensions))
    scene.render.resolution_x = max(1, round(source_dimensions[0] * scale))
    scene.render.resolution_y = max(1, round(source_dimensions[1] * scale))
    scene.cycles.samples = min(scene.cycles.samples, 32)
source_hash = file_digest(bpy.data.filepath)
if camera not in bpy.data.objects or bpy.data.objects[camera].type != "CAMERA":
    raise ValueError(f"Camera missing: {camera}")
scene.camera = bpy.data.objects[camera]
scene.render.engine = "CYCLES"
prefs = bpy.context.preferences.addons["cycles"].preferences
enabled = []
if device == "METAL":
    prefs.compute_device_type = "METAL"
    prefs.get_devices()
    for item in prefs.devices:
        item.use = item.type == "METAL"
        if item.use:
            enabled.append({"name": item.name, "type": item.type, "id": item.id})
    if not enabled:
        raise RuntimeError("Metal requested but no Metal device available")
    scene.cycles.device = "GPU"
elif device == "CPU":
    scene.cycles.device = "CPU"
    enabled = [{"name": "CPU", "type": "CPU"}]
else:
    raise ValueError("Unsupported render device")
output.mkdir(parents=True, exist_ok=True)
print("CYCLES_DEVICE", json.dumps(enabled), flush=True)
for current_frame in range(frame, last_frame + 1):
    scene.frame_set(current_frame)
    stem = f"frame_{current_frame:06d}"
    scene.render.filepath = str(output / (stem + ".png"))
    scene.render.image_settings.media_type = "IMAGE"
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_depth = "16"
    start = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    elapsed = time.perf_counter() - start
    scene.render.image_settings.media_type = "MULTI_LAYER_IMAGE"
    scene.render.image_settings.file_format = "OPEN_EXR_MULTILAYER"
    scene.render.image_settings.color_depth = "32"
    bpy.data.images["Render Result"].save_render(str(output / (stem + ".exr")), scene=scene)
    report = {"status": "RENDERED", "engine": scene.render.engine,
              "quality_scope": "PREVIEW_NOT_FINAL" if profile == "PREVIEW" else "AUTHORED_SCENE_SETTINGS",
              "source_dimensions": source_dimensions,
              "device_requested": device, "devices_enabled": enabled, "camera": camera,
              "frame": current_frame, "width": scene.render.resolution_x,
              "height": scene.render.resolution_y, "samples_limit": scene.cycles.samples,
              "resolution_percentage": scene.render.resolution_percentage,
              "png_bit_depth": 16, "exr_bit_depth": 32,
              "camera_optics": {"lens_mm": scene.camera.data.lens,
                  "sensor_width_mm": scene.camera.data.sensor_width,
                  "depth_of_field": scene.camera.data.dof.use_dof,
                  "f_stop": scene.camera.data.dof.aperture_fstop,
                  "focus_distance_m": scene.camera.data.dof.focus_distance},
              "render_seconds": elapsed, "png": stem + ".png", "aovs": stem + ".exr",
              "source_blend": bpy.data.filepath, "source_sha256": source_hash,
              "blender_version": bpy.app.version_string}
    (output / (stem + ".json")).write_text(json.dumps(report, indent=2))
    print("CYCLES_RENDER_COMPLETE", json.dumps(report), flush=True)
    
