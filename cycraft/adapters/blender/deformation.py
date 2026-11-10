"""Bounded evaluated deformation bake for serialized Blender/GLB/USD products."""
import bpy
import hashlib
import json
import math
import struct
from array import array
from contextlib import contextmanager

MAX_VERTEX_FRAMES = 2000000
STATIC = {"BEVEL", "SUBSURF", "SOLIDIFY", "TRIANGULATE", "WEIGHTED_NORMAL",
          "NORMAL_EDIT", "EDGE_SPLIT", "MIRROR", "WELD", "DECIMATE", "MULTIRES", "COLLISION"}
DEFORM = {"CLOTH", "SOFT_BODY", "WAVE", "SIMPLE_DEFORM", "DISPLACE", "CAST",
          "LATTICE", "MESH_DEFORM", "SURFACE_DEFORM", "MESH_CACHE", "MESH_SEQUENCE_CACHE"}

def action_curves(action):
    if action is None:
        return
    if hasattr(action, "fcurves"):
        yield from action.fcurves
    else:
        for layer in action.layers:
            for strip in layer.strips:
                for bag in getattr(strip, "channelbags", ()):
                    yield from bag.fcurves

def modifier_animated(obj):
    animation = obj.animation_data
    if animation is None:
        return False
    if animation.nla_tracks:
        raise ValueError("NLA modifier animation requires explicit baking before export")
    found = False
    for curves in (animation.drivers, action_curves(animation.action)):
        for index, curve in enumerate(curves):
            if index >= 4096:
                raise ValueError("Modifier animation curve budget exceeded")
            if curve.data_path.startswith("modifiers["):
                if curve.data_path.endswith(("show_viewport", "show_render")):
                    raise ValueError("Animated modifier visibility cannot be frozen safely")
                found = True
    return found

def topology(mesh):
    digest = hashlib.sha256()
    digest.update(struct.pack("<4Q", len(mesh.vertices), len(mesh.edges), len(mesh.loops), len(mesh.polygons)))
    for edge in mesh.edges:
        digest.update(struct.pack("<2I", *edge.vertices))
    for loop in mesh.loops:
        digest.update(struct.pack("<I", loop.vertex_index))
    for polygon in mesh.polygons:
        digest.update(struct.pack("<3I", polygon.loop_start, polygon.loop_total, polygon.material_index))
    digest.update(struct.pack("<I", len(mesh.materials)))
    for material in mesh.materials:
        identity = material.name.encode("utf8") if material else b""
        digest.update(struct.pack("<I", len(identity)))
        digest.update(identity)
    for layer in mesh.uv_layers:
        digest.update(layer.name.encode("utf8"))
        for item in layer.data:
            digest.update(struct.pack("<2f", *item.uv))
    return digest.hexdigest()

def positions(mesh):
    coordinates = array("f", [0]) * (len(mesh.vertices) * 3)
    mesh.vertices.foreach_get("co", coordinates)
    if any(not math.isfinite(value) or abs(value) > 10000 for value in coordinates):
        raise ValueError("Nonfinite or out-of-bounds deformation coordinates")
    return coordinates

@contextmanager
def bake_deformations(scene):
    first, last, original_frame = scene.frame_start, scene.frame_end, scene.frame_current
    if not 1 <= first <= last <= 999999 or last - first >= 240:
        raise ValueError("Deformation frame budget exceeded")
    if len(scene.objects) > 10000:
        raise ValueError("Deformation scene object budget exceeded")
    selected, preserved_rigs = [], []
    for obj in scene.objects:
        if obj.type != "MESH" or not obj.modifiers:
            continue
        kinds = {m.type for m in obj.modifiers if m.show_viewport or m.show_render}
        # Existing rig/shape-key exports retain their modifiers and drivers.
        # Visibility rejection applies only to stacks this context will freeze.
        if "ARMATURE" in kinds or obj.data.shape_keys:
            if kinds & {"CLOTH", "SOFT_BODY", "MESH_CACHE", "MESH_SEQUENCE_CACHE"}:
                raise ValueError(f"Mixed rig/shape-key and modifier deformation requires a separate bake: {obj.name}")
            preserved_rigs.append(obj.name)
            continue
        animated = modifier_animated(obj)
        if kinds - STATIC - DEFORM - {"ARMATURE"}:
            raise ValueError(f"Unsupported modifier export on {obj.name}: {sorted(kinds)}")
        if not (kinds & DEFORM or animated):
            continue
        if any(m.show_viewport != m.show_render for m in obj.modifiers):
            raise ValueError(f"Viewport/render modifier mismatch: {obj.name}")
        if len(selected) >= 32:
            raise ValueError("Deforming object budget exceeded")
        selected.append({"object": obj, "original": obj.data, "mesh": None, "frames": [],
                         "flags": [(m, m.show_viewport, m.show_render) for m in obj.modifiers],
                         "applied": False})
    report = {"schema": "singular.deformation/1", "status": "NO_DEFORMING_MODIFIERS",
              "frame_start": first, "frame_end": last, "objects": [],
              "preserved_existing_rig_path": preserved_rigs,
              "scope": "Fixed-topology geometry only; existing rig modifier and animated shader parity not established"}
    try:
        vertex_frames = 0
        graph = bpy.context.evaluated_depsgraph_get()
        for frame in range(first, last + 1) if selected else ():
            scene.frame_set(frame)
            for entry in selected:
                obj = entry["object"]
                evaluated = obj.evaluated_get(graph)
                mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=graph)
                try:
                    count = len(mesh.vertices)
                    if frame == first:
                        vertex_frames += count * (last - first + 1)
                        if count == 0 or vertex_frames > MAX_VERTEX_FRAMES:
                            raise ValueError("Deformation vertex-frame budget exceeded")
                        entry["topology"] = topology(mesh)
                        if any(material and (material.animation_data or
                                material.node_tree and material.node_tree.animation_data)
                               for material in mesh.materials):
                            raise ValueError("Animated deformation materials require a separate export path")
                        entry["mesh"] = bpy.data.meshes.new_from_object(evaluated,
                            preserve_all_data_layers=True, depsgraph=graph)
                        pins = json.loads(obj.get("singular_cloth_pins", "[]"))
                        if not isinstance(pins, list) or len(pins) > count or any(type(i) is not int or not 0 <= i < count for i in pins):
                            raise ValueError("Invalid cloth pin evidence indices")
                        entry["pins"] = pins
                    elif topology(mesh) != entry["topology"]:
                        raise ValueError(f"Deformation changes topology or UV/material layout: {obj.name}")
                    entry["frames"].append(positions(mesh))
                finally:
                    evaluated.to_mesh_clear()
        for entry in selected:
            obj, base = entry["object"], entry["mesh"]
            obj.data = base
            entry["applied"] = True
            for modifier, _, _ in entry["flags"]:
                modifier.show_viewport = modifier.show_render = False
            obj.shape_key_add(name="Basis", from_mix=False)
            samples = []
            reference = entry["frames"][0]
            for offset, coords in enumerate(entry["frames"]):
                frame = first + offset
                if offset:
                    key = obj.shape_key_add(name=f"Evaluated_{frame:06d}", from_mix=False)
                    key.data.foreach_set("co", coords)
                    for when, value in ((frame - 1, 0), (frame, 1), (frame + 1, 0)):
                        key.value = value
                        key.keyframe_insert(data_path="value", frame=when)
                distances = (math.sqrt(sum((coords[i + axis] - reference[i + axis]) ** 2 for axis in range(3)))
                             for i in range(0, len(coords), 3))
                pin_error = max((math.sqrt(sum((coords[i * 3 + axis] - reference[i * 3 + axis]) ** 2
                    for axis in range(3))) for i in entry["pins"]), default=0)
                samples.append({"frame": frame, "positions_sha256": hashlib.sha256(coords.tobytes()).hexdigest(),
                                "max_displacement_m": max(distances), "max_pin_error_m": pin_error})
            if base.shape_keys.animation_data:
                for curve in action_curves(base.shape_keys.animation_data.action):
                    for point in curve.keyframe_points:
                        point.interpolation = "LINEAR"
            report["objects"].append({"name": obj.name, "vertices": len(base.vertices),
                "topology_sha256": entry["topology"], "pin_count": len(entry["pins"]),
                "source_modifiers": [m.type for m, _, _ in entry["flags"]], "samples": samples})
        if selected:
            report["status"] = "EVALUATED_DEFORMATION_BAKED"
        scene.frame_set(original_frame)
        yield report
    finally:
        for entry in reversed(selected):
            obj, mesh = entry["object"], entry["mesh"]
            action = mesh.shape_keys.animation_data.action if mesh and mesh.shape_keys and mesh.shape_keys.animation_data else None
            if entry["applied"]:
                obj.data = entry["original"]
                for modifier, viewport, render in entry["flags"]:
                    modifier.show_viewport, modifier.show_render = viewport, render
            if mesh:
                bpy.data.meshes.remove(mesh)
            if action and action.users == 0:
                bpy.data.actions.remove(action)
        scene.frame_set(original_frame)
