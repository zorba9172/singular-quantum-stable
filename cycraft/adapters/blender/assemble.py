"""Execute a native-authored scene contract through Blender; no city generation here."""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from materials import make_material
from geometry import box, indexed_mesh
from cloth import collection_overrides, configure_cloth
from scene_io import import_prototype, export_scene, append_collection, file_digest
from asset_material_overrides import EVIDENCE_KEY
from world_environment import configure_world, environment_state

MAX_SCENE_OBJECTS = 10000

def object_cost(spec, prototypes):
    return len(prototypes[spec["asset"]]) if spec["kind"] == "asset" else 1

def orient(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()

def focus_camera(data, location, target):
    distance = (Vector(target) - Vector(location)).length
    if not math.isfinite(distance) or not 0.01 <= distance <= 10000:
        raise ValueError("Camera focus distance outside production bounds")
    data.dof.focus_distance = distance

def rigid_body(obj, spec):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.rigidbody.object_add()
    obj.rigid_body.type = spec["type"]
    obj.rigid_body.collision_shape = "BOX"
    obj.rigid_body.friction = spec.get("friction", 0.65)
    obj.rigid_body.restitution = 0.05
    obj.rigid_body.use_margin = True
    obj.rigid_body.collision_margin = 0.003
    if spec["type"] == "ACTIVE":
        obj.rigid_body.mass = spec["mass_kg"]

def assemble(contract, output):
    if contract.get("schema") != "singular.scene/1" or contract.get("up_axis") != "Z":
        raise ValueError("Unsupported scene contract")
    if len(contract["objects"]) > MAX_SCENE_OBJECTS:
        raise ValueError("Object budget exceeded")
    start, end = contract["frame_start"], contract["frame_end"]
    if type(start) is not int or type(end) is not int or not 1 <= start <= end <= 999999 or end-start >= 240:
        raise ValueError("Scene frame budget exceeded")
    if sum("physics" in obj for obj in contract["objects"]) * (end-start+1) > 100000:
        raise ValueError("Physics transform budget exceeded")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1
    scene.render.fps = contract["fps"]
    scene.frame_start, scene.frame_end = contract["frame_start"], contract["frame_end"]
    settings = contract["render"]
    scene.render.engine = "CYCLES"
    scene.cycles.samples = settings["samples"]
    scene.cycles.use_adaptive_sampling = True
    scene.cycles.adaptive_threshold = settings["adaptive_threshold"]
    scene.cycles.use_denoising = settings["denoise"]
    scene.cycles.max_bounces = 8
    scene.cycles.diffuse_bounces = 3
    scene.cycles.glossy_bounces = 4
    scene.cycles.transmission_bounces = 4
    scene.render.resolution_x, scene.render.resolution_y = settings["width"], settings["height"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.exposure = settings["exposure"]
    configure_world(contract["world"], file_digest)
    materials = {s["id"]: make_material(s) for s in contract["materials"]}
    prototypes = {s["id"]: import_prototype(s) for s in contract["assets"]}
    expanded = sum(object_cost(s, prototypes) for s in contract["objects"])
    if expanded > MAX_SCENE_OBJECTS:
        raise ValueError("Expanded asset instance object budget exceeded")
    dynamic = []
    objects = {}
    for spec in contract["objects"]:
        if len(scene.objects) + object_cost(spec, prototypes) > MAX_SCENE_OBJECTS:
            raise ValueError("Scene object expansion budget exceeded")
        if spec["kind"] == "blend_collection":
            root = append_collection(spec)
            if len(scene.objects) > MAX_SCENE_OBJECTS:
                raise ValueError("Appended collection exceeds scene object budget")
            objects[spec["id"]] = root
            collection_overrides(root, spec, materials)
            continue
        if spec["kind"] == "asset":
            for index, mesh in enumerate(prototypes[spec["asset"]]):
                obj = bpy.data.objects.new(f'{spec["id"]}_{index}', mesh)
                scene.collection.objects.link(obj)
                obj.location = spec["location"]
                obj.rotation_euler.z = math.radians(spec["yaw_degrees"])
            continue
        if spec["kind"] == "box":
            obj = box(spec)
        elif spec["kind"] == "mesh":
            obj = indexed_mesh(spec, spec["vertices"], spec["faces"])
        elif spec["kind"] == "sphere":
            bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12)
            obj = bpy.context.object
            obj.name = spec["id"]
            # Dimensions are baked into mesh; physics sees meter-sized geometry.
            for vertex in obj.data.vertices:
                vertex.co = Vector(tuple(vertex.co[i] * spec["size"][i] * 0.5 for i in range(3)))
            for polygon in obj.data.polygons:
                polygon.use_smooth = True
        else:
            raise ValueError(f'Unknown geometry kind: {spec["kind"]}')
        obj.location = spec["location"]
        objects[spec["id"]] = obj
        obj.data.materials.append(materials[spec["material"]])
        if "physics" in spec:
            rigid_body(obj, spec["physics"])
            dynamic.append(obj)
        elif spec["id"] in ("road", "cross_street") or spec["id"].startswith(("walk_", "paving_")):
            rigid_body(obj, {"type": "PASSIVE", "friction": 0.7})
    for spec in contract["lights"]:
        data = bpy.data.lights.new(spec["id"], spec["kind"])
        data.energy, data.color, data.size = spec["power_watts"], spec["color"], spec["size_m"]
        obj = bpy.data.objects.new(spec["id"], data)
        scene.collection.objects.link(obj)
        obj.location = spec["location"]
        orient(obj, spec["target"])
    for spec in contract["cameras"]:
        data = bpy.data.cameras.new(spec["id"])
        for field, lower, upper in (("lens_mm", 1, 500), ("f_stop", 0.7, 64)):
            value = spec[field]
            if type(value) not in (int, float) or not math.isfinite(value) or not lower <= value <= upper:
                raise ValueError("Camera optical parameter outside production bounds")
        data.lens = spec["lens_mm"]
        data.dof.use_dof = True
        data.dof.aperture_fstop = spec["f_stop"]
        focus_camera(data, spec["location"], spec["target"])
        data.clip_end = 300
        obj = bpy.data.objects.new(spec["id"], data)
        scene.collection.objects.link(obj)
        obj.location = spec["location"]
        orient(obj, spec["target"])
        keys = spec.get("keyframes", [])
        if len(keys) > 240:
            raise ValueError("Camera keyframe budget exceeded")
        for key in keys:
            if not scene.frame_start <= key["frame"] <= scene.frame_end:
                raise ValueError("Camera keyframe is outside scene range")
            obj.location = key["location"]
            orient(obj, key["target"])
            focus_camera(data, key["location"], key["target"])
            data.keyframe_insert(data_path="dof.focus_distance", frame=key["frame"])
            obj.keyframe_insert(data_path="location", frame=key["frame"])
            obj.keyframe_insert(data_path="rotation_euler", frame=key["frame"])
        if spec["id"] == settings["camera"]:
            scene.camera = obj
    if scene.camera is None:
        raise ValueError("Active camera is missing")
    physics = {"engine": "Blender Bullet rigid body", "samples": [], "baked": False}
    if dynamic:
        scene.rigidbody_world.substeps_per_frame = 10
        scene.rigidbody_world.solver_iterations = 20
        scene.rigidbody_world.point_cache.frame_start = scene.frame_start
        scene.rigidbody_world.point_cache.frame_end = scene.frame_end
        transforms = {obj.name: [] for obj in dynamic}
        for frame in range(scene.frame_start, scene.frame_end + 1):
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            depsgraph = bpy.context.evaluated_depsgraph_get()
            for obj in dynamic:
                transforms[obj.name].append((frame, obj.evaluated_get(depsgraph).matrix_world.copy()))
        # Record native Bullet results as ordinary keys so workers need no mutable cache.
        for obj in dynamic:
            bpy.ops.object.select_all(action="DESELECT")
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            bpy.ops.rigidbody.object_remove()
            obj.rotation_mode = "QUATERNION"
            for frame, matrix in transforms[obj.name]:
                obj.location, obj.rotation_quaternion, obj.scale = matrix.decompose()
                obj.keyframe_insert(data_path="location", frame=frame)
                obj.keyframe_insert(data_path="rotation_quaternion", frame=frame)
        physics["baked"] = True
        for frame in sorted({start, (start+end)//2, end}):
            scene.frame_set(frame)
            physics["samples"].append({"frame": frame, "positions":
                {obj.name: list(obj.matrix_world.translation) for obj in dynamic}})
    scene.frame_set(start if any("cloth" in spec for spec in contract["objects"]) else end)
    physics["cloth"] = configure_cloth(scene, contract, objects)
    layer = scene.view_layers[0]
    layer.use_pass_z = True
    layer.use_pass_normal = True
    layer.use_pass_diffuse_color = True
    layer.use_pass_diffuse_direct = True
    layer.use_pass_diffuse_indirect = True
    layer.use_pass_glossy_direct = True
    layer.use_pass_glossy_indirect = True
    inventory = export_scene(output)
    report = {"status": "ASSEMBLED_NOT_RENDERED", "inventory": inventory,
              "physics": physics, "source_contract": contract["schema"],
              "render_device_verified": False,
              "environment": environment_state(scene.world),
              "asset_material_overrides": json.loads(scene.get(EVIDENCE_KEY, "[]"))}
    (Path(output) / "assembly.json").write_text(json.dumps(report, indent=2))
    print("SCENE_ASSEMBLED", json.dumps(inventory), flush=True)

if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--")+1:]
    if len(args) != 2:
        raise SystemExit("Expected contract.json OUTPUT_DIRECTORY")
    assemble(json.loads(Path(args[0]).read_text()), Path(args[1]).resolve())

