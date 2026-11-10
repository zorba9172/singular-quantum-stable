"""Bound optical state that glTF does not transport, sampled at film frames."""
import math

FIELDS = {"lens": (1, 500), "sensor_width": (1, 100), "sensor_height": (1, 100),
          "shift_x": (-10, 10), "shift_y": (-10, 10),
          "dof.aperture_fstop": (0.7, 64), "dof.focus_distance": (0.01, 10000)}

def property_owner(data, field):
    return (data.dof, field[4:]) if field.startswith("dof.") else (data, field)

def capture_cameras(scene):
    cameras = [obj for obj in scene.objects if obj.type == "CAMERA"]
    if not cameras:
        return []
    if len(cameras) > 64 or not 1 <= scene.frame_end-scene.frame_start+1 <= 240:
        raise ValueError("Camera optical sample budget exceeded")
    records = [{"name": obj.name, "use_dof": obj.data.dof.use_dof,
                "sensor_fit": obj.data.sensor_fit, "samples": []} for obj in cameras]
    original = scene.frame_current
    try:
        for frame in range(scene.frame_start, scene.frame_end+1):
            scene.frame_set(frame)
            for obj, record in zip(cameras, records):
                if obj.data.dof.focus_object is not None:
                    raise ValueError("Object-bound focus requires baking before interchange")
                values = {}
                for field in FIELDS:
                    owner, name = property_owner(obj.data, field)
                    values[field] = getattr(owner, name)
                record["samples"].append({"frame": frame, "values": values})
    finally:
        scene.frame_set(original)
    return records

def restore_cameras(scene, records):
    if len(records) > 64 or len({r["name"] for r in records}) != len(records):
        raise ValueError("Invalid camera optical package")
    original = scene.frame_current
    for record in records:
        obj = scene.objects.get(record["name"])
        if obj is None or obj.type != "CAMERA":
            raise ValueError("Bound optical camera missing")
        samples = record["samples"]
        if [s["frame"] for s in samples] != list(range(scene.frame_start, scene.frame_end+1)):
            raise ValueError("Incomplete camera optical samples")
        if type(record["use_dof"]) is not bool or record["sensor_fit"] not in ("AUTO", "HORIZONTAL", "VERTICAL"):
            raise ValueError("Invalid camera optical mode")
        obj.data.dof.focus_object = None
        obj.data.dof.use_dof = record["use_dof"]
        obj.data.sensor_fit = record["sensor_fit"]
        for sample in samples:
            if set(sample["values"]) != set(FIELDS):
                raise ValueError("Invalid camera optical fields")
            for field, (lower, upper) in FIELDS.items():
                value = sample["values"][field]
                if type(value) not in (int, float) or not math.isfinite(value) or not lower <= value <= upper:
                    raise ValueError("Camera optical value outside production bounds")
                owner, name = property_owner(obj.data, field)
                setattr(owner, name, value)
                obj.data.keyframe_insert(data_path=field, frame=sample["frame"])
    scene.frame_set(original)
