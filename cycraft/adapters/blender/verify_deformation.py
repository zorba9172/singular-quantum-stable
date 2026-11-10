"""Verify sampled motion transport of an explicitly expected animated pinned mesh."""
import bpy
import json
import re
import sys
from pathlib import Path
from mathutils import Vector
from mathutils.kdtree import KDTree

sys.path.insert(0, str(Path(__file__).parent))
from animation import sample_mesh, sampled_frames
from scene_io import file_digest, validate_asset
from interchange import restore_render_state

MAX_VERTICES = 100000
TOLERANCE_M = 0.00001
MIN_MOTION_M = 0.0001


def identify_mesh(scene, name, native):
    if len(scene.objects) > 10000:
        raise ValueError("Imported object budget exceeded")
    pattern = re.compile(re.escape(name) + r"(?:[._][0-9]{3})?")
    matches = [obj for obj in scene.objects if obj.type == "MESH" and
               (obj.name == name if native else bool(pattern.fullmatch(obj.name)))]
    if len(matches) != 1:
        raise ValueError("Expected exactly one mesh matching the requested name")
    return matches[0]


def nearest_error(source, target):
    tree = KDTree(len(target))
    for index, point in enumerate(target):
        tree.insert(point, index)
    tree.balance()
    return max(tree.find(point)[2] for point in source)


def verify(directory, name, result):
    artifacts = {suffix: directory / ("nocturne." + suffix)
                 for suffix in ("blend", "glb", "usdc")}
    for source in artifacts.values():
        if not source.is_file() or not 0 < source.stat().st_size <= 2*1024**3:
            raise ValueError("Missing artifact or artifact exceeds 2 GiB import budget")
    reference, pins, frames = {}, None, None
    for suffix, source in artifacts.items():
        bpy.ops.wm.read_factory_settings(use_empty=True)
        if suffix == "blend":
            bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
        elif suffix == "glb":
            validate_asset(source)
            bpy.ops.import_scene.gltf(filepath=str(source))
            restored = restore_render_state(source, file_digest)
            if restored.get("status") == "UNAVAILABLE":
                raise ValueError("Bound render-state companion required for GLB timing")
        else:
            bpy.ops.wm.usd_import(filepath=str(source))
        scene = bpy.context.scene
        obj = identify_mesh(scene, name, suffix == "blend")
        if suffix == "blend":
            frames = sampled_frames(scene)
            if len(frames) < 2:
                raise ValueError("Animated-mesh verification requires multiple frames")
            raw = obj.get("singular_cloth_pins")
            if not isinstance(raw, str) or len(raw) > 65536:
                raise ValueError("Missing or oversized source pin metadata")
            pins = json.loads(raw)
            if (not isinstance(pins, list) or not 1 <= len(pins) <= MAX_VERTICES or
                    any(type(index) is not int or index < 0 for index in pins) or
                    len(set(pins)) != len(pins)):
                raise ValueError("Invalid source pin indices")
            result.update({"frames": frames, "pin_indices": pins})
        item = {"sha256": file_digest(source), "object": obj.name,
            "shape_key_blocks": len(obj.data.shape_keys.key_blocks) if obj.data.shape_keys else 0,
            "active_simulation_modifiers": [m.type for m in obj.modifiers
                if m.type in {"CLOTH", "SOFT_BODY"} and (m.show_viewport or m.show_render)],
            "samples": []}
        result["artifacts"][suffix] = item
        if item["active_simulation_modifiers"]:
            raise ValueError("Expected durable baked mesh, found active simulation modifiers")
        first, last = None, None
        for frame in frames:
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            coordinates, sample = sample_mesh(obj, bpy.context.evaluated_depsgraph_get(), MAX_VERTICES)
            points = [Vector(coordinates[i:i+3]) for i in range(0, len(coordinates), 3)]
            if first is None:
                first = points
            if len(first) != len(points):
                raise ValueError("Within-artifact evaluated vertex count changed")
            sample["frame"] = frame
            sample["max_displacement_from_start_m"] = max((a-b).length for a, b in zip(first, points))
            if suffix == "blend":
                if max(pins) >= len(points):
                    raise ValueError("Source pin index outside evaluated mesh")
                reference[frame] = points
                sample["max_pin_motion_m"] = max((points[i]-reference[frames[0]][i]).length for i in pins)
            else:
                sample["max_bidirectional_source_distance_m"] = max(
                    nearest_error(points, reference[frame]), nearest_error(reference[frame], points))
                sample["pin_source_match_error_m"] = nearest_error([reference[frame][i] for i in pins], points)
            item["samples"].append(sample)
            last = points
        item["max_vertex_motion_first_last_m"] = max((a-b).length for a, b in zip(first, last))
        item["motion_present"] = (max(s["max_displacement_from_start_m"] for s in item["samples"]) > MIN_MOTION_M
            and len({s["local_geometry_sha256"] for s in item["samples"]}) > 1)
    for suffix in ("glb", "usdc"):
        result[suffix + "_parity_passed"] = all(
            s["max_bidirectional_source_distance_m"] <= TOLERANCE_M and
            s["pin_source_match_error_m"] <= TOLERANCE_M
            for s in result["artifacts"][suffix]["samples"])
    result["motion_present"] = all(item["motion_present"] for item in result["artifacts"].values())
    result["passed"] = result["glb_parity_passed"] and result["usdc_parity_passed"] and result["motion_present"]
    result["status"] = "SAMPLED_DEFORMATION_TRANSPORT_VERIFIED" if result["passed"] else "FAILED"


args = sys.argv[sys.argv.index("--") + 1:]
if len(args) != 3:
    raise SystemExit("Expected SCENE_DIR MESH_NAME NEW_REPORT.json")
directory, name, destination = Path(args[0]).resolve(), args[1], Path(args[2]).resolve()
if not directory.is_dir() or not name or len(name) > 128 or destination.exists():
    raise ValueError("Expected existing scene directory, bounded mesh name and new report")
result = {"schema": "singular.deformation-verification/1", "status": "FAILED", "passed": False,
    "mesh": name, "artifacts": {}, "tolerance_m": TOLERANCE_M, "minimum_motion_m": MIN_MOTION_M,
    "simulation_quality_verified": False,
    "method": "Bidirectional nearest evaluated world-space vertex positions permit format vertex splits/reordering",
    "limits": {"vertices_per_mesh": MAX_VERTICES, "sample_frames": 3, "scene_frame_span": 240,
               "artifact_bytes": 2*1024**3, "imported_objects": 10000},
    "limitations": ["Three integer-frame observations, not every frame or subframe",
        "Position-set parity does not prove face connectivity, normals, materials or UV parity",
        "Within-format displacement assumes stable vertex ordering",
        "Pin correspondence uses nearest positions, not exported vertex identity",
        "Does not verify cloth-body penetration, attachment quality or simulation accuracy"]}
destination.parent.mkdir(parents=True, exist_ok=True)
with destination.open("x") as stream:
    try:
        verify(directory, name, result)
    except Exception as error:
        result.update({"status": "FAILED", "passed": False, "error": str(error)[:2048]})
    json.dump(result, stream, indent=2)
print("DEFORMATION_VERIFIED", json.dumps(result), flush=True)
if not result["passed"]:
    raise RuntimeError("Deformation verification failed; see report")
