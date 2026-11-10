"""Preserve render state outside glTF's supported light model in a bound package."""
import bpy
import json
from pathlib import Path
from mathutils import Matrix
from camera_state import capture_cameras, restore_cameras
from world_environment import environment_state, restore_environment, configure_world, world_background

def render_state(output=None):
    scene = bpy.context.scene
    lights = []
    for obj in scene.objects:
        if obj.type == "LIGHT" and obj.data.type == "AREA":
            data = obj.data
            lights.append({"name": obj.get("source_render_name", obj.name), "matrix_world": [list(row) for row in obj.matrix_world],
                "color": list(data.color), "energy": data.energy, "shape": data.shape,
                "size": data.size, "size_y": data.size_y, "spread": data.spread})
    if len(lights) > 1024:
        raise ValueError("Area light package budget exceeded")
    world = None
    background = world_background(scene.world)
    if background and not background.inputs["Color"].is_linked:
        world = {"color": list(background.inputs["Color"].default_value),
                 "strength": background.inputs["Strength"].default_value}
    return {"area_lights": lights, "constant_world": world,
        "environment_world": environment_state(scene.world, output),
        "frame_start": scene.frame_start, "frame_end": scene.frame_end,
        "fps": scene.render.fps, "fps_base": scene.render.fps_base,
        "camera": scene.camera.name if scene.camera else None,
        "camera_optics": capture_cameras(scene),
        "width": scene.render.resolution_x, "height": scene.render.resolution_y,
        "resolution_percentage": scene.render.resolution_percentage,
        "view_transform": scene.view_settings.view_transform,
        "exposure": scene.view_settings.exposure}

def export_render_state(output, digest):
    output = Path(output)
    report = {"schema": "singular.interchange/1", "coordinate_system": "BLENDER_Z_UP_METERS",
        "artifacts": {name: digest(output / name) for name in
                      ("nocturne.blend", "nocturne.glb", "nocturne.usdc")},
        "render_state": render_state(output),
        "limitations": ["Procedural shader parity is not verified",
            "Constant worlds and content-pinned unrotated HDR environments are restored; arbitrary world graphs are unsupported",
            "Area light transforms and power are static snapshots; animated lighting is not restored",
            "Camera optics are sampled at integer film frames; subframe optical parity is not verified",
            "Rig and deformation parity must be verified separately"]}
    with (output / "interchange.json").open("x") as stream:
        json.dump(report, stream, indent=2)

def restore_render_state(source, digest):
    source = Path(source)
    companion = source.parent / "interchange.json"
    if not companion.exists():
        return {"status": "UNAVAILABLE", "reason": "No bound render-state companion"}
    if companion.stat().st_size > 1024 * 1024:
        raise ValueError("Render-state companion budget exceeded")
    report = json.loads(companion.read_text())
    if report.get("schema") != "singular.interchange/1" or report.get("coordinate_system") != "BLENDER_Z_UP_METERS":
        raise ValueError("Unsupported render-state schema")
    if report["artifacts"].get(source.name) != digest(source):
        raise ValueError("Render-state artifact hash mismatch")
    state = report["render_state"]
    if len(state["area_lights"]) > 1024 or not 1 <= state["frame_end"] - state["frame_start"] + 1 <= 240:
        raise ValueError("Render-state scene budget exceeded")
    scene = bpy.context.scene
    for spec in state["area_lights"]:
        obj = scene.objects.get(spec["name"])
        if obj is not None and obj.type not in ("EMPTY", "LIGHT"):
            raise ValueError("Area light name collides with imported geometry")
        if obj is not None and obj.type == "EMPTY":
            # Blender cannot change an Empty into a Light by assigning its data.
            # Preserve the imported hierarchy and restore a separate light object.
            obj = scene.objects.get(spec["name"] + "__render_area")
        if obj is not None and (obj.type != "LIGHT" or obj.data.type != "AREA"):
            raise ValueError("Restored area light name collision")
        if obj is None:
            data = bpy.data.lights.new(spec["name"], "AREA")
            obj = bpy.data.objects.new(spec["name"] + "__render_area", data)
            scene.collection.objects.link(obj)
        data = obj.data
        obj["source_render_name"] = spec["name"]
        obj.matrix_world = Matrix(spec["matrix_world"])
        for field in ("color", "energy", "shape", "size", "size_y", "spread"):
            setattr(data, field, spec[field])
    world = state["constant_world"]
    environment = state.get("environment_world")
    if world is not None and environment is not None:
        raise ValueError("Ambiguous constant and HDR world state")
    if environment is not None:
        restore_environment(environment, source.parent, digest)
    elif world is not None:
        configure_world(world, digest)
    scene.frame_start, scene.frame_end = state["frame_start"], state["frame_end"]
    for field in ("fps", "fps_base", "resolution_percentage"):
        setattr(scene.render, field, state[field])
    scene.render.resolution_x, scene.render.resolution_y = state["width"], state["height"]
    scene.view_settings.view_transform = state["view_transform"]
    scene.view_settings.exposure = state["exposure"]
    scene.camera = scene.objects.get(state["camera"]) if state["camera"] else None
    if state["camera"] and (scene.camera is None or scene.camera.type != "CAMERA"):
        raise ValueError("Bound camera missing from imported scene")
    if "camera_optics" in state:
        restore_cameras(scene, state["camera_optics"])
    return {"status": "RESTORED_BOUND_RENDER_STATE", "limitations": report["limitations"]}
