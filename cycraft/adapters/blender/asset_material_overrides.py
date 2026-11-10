"""Exact-name, copy-on-write overrides of imported prototype materials."""
import json
import math
import struct
import bpy
from material_observation import surface_nodes, MAX_LINKS

EVIDENCE_KEY = "singular_asset_material_overrides"
CONTROLS = {"normal_strength": ("NORMAL_MAP", "Strength"),
            "roughness": ("BSDF_PRINCIPLED", "Roughness"),
            "coat_roughness": ("BSDF_PRINCIPLED", "Coat Roughness")}


def source_material_names(path):
    if path.suffix == ".glb":
        with path.open("rb") as stream:
            stream.seek(12)
            header = stream.read(8)
            if len(header) != 8:
                raise ValueError("Missing GLB material metadata")
            length, kind = struct.unpack("<II", header)
            if kind != 0x4E4F534A or not 0 < length <= 4*1024*1024:
                raise ValueError("GLB material metadata budget exceeded")
            payload = stream.read(length)
            if len(payload) != length:
                raise ValueError("Truncated GLB material metadata")
    else:
        if path.stat().st_size > 4*1024*1024:
            raise ValueError("glTF material metadata budget exceeded")
        payload = path.read_bytes()
    document = json.loads(payload)
    materials = document.get("materials", [])
    if not isinstance(materials, list) or len(materials) > 1024:
        raise ValueError("Source material inventory budget exceeded")
    return [item.get("name") for item in materials]


def apply_asset_material_overrides(meshes, spec, path):
    overrides = spec.get("material_overrides", [])
    if not isinstance(overrides, list) or len(overrides) > 32:
        raise ValueError("Asset material override budget exceeded")
    if not overrides:
        return
    if len(meshes) > 2048 or any(len(mesh.materials) > 64 for mesh in meshes):
        raise ValueError("Prototype material binding budget exceeded")
    attached = {material for mesh in meshes for material in mesh.materials if material}
    names = source_material_names(path)
    planned, seen = [], set()
    for override in overrides:
        if (not isinstance(override, dict) or "material" not in override
                or not 2 <= len(override) <= 4 or set(override) - {"material", *CONTROLS}):
            raise ValueError("Unsupported asset material override fields")
        name = override["material"]
        if not isinstance(name, str) or not 1 <= len(name) <= 128 or name in seen:
            raise ValueError("Invalid or repeated asset material override name")
        seen.add(name)
        # Validate source identity and exact imported name; never strip Blender's collision suffixes.
        matches = [material for material in attached if material.name == name]
        renamed = [material for material in attached if material.name.startswith(name + ".")]
        if names.count(name) != 1 or len(matches) != 1 or renamed:
            raise ValueError("Missing or ambiguous exact imported material: " + name)
        material = matches[0]
        if not material.use_nodes or material.node_tree is None or len(material.node_tree.nodes) > 128:
            raise ValueError("Override requires a bounded material node graph")
        if len(material.node_tree.links) > MAX_LINKS:
            raise ValueError("Override material link budget exceeded")
        active = surface_nodes(material.node_tree.nodes)
        edits = []
        for control, (node_type, input_name) in CONTROLS.items():
            if control not in override:
                continue
            value = override[control]
            if type(value) not in (int, float) or not math.isfinite(value) or not 0 <= value <= 1:
                raise ValueError("Asset " + control + " must be finite in [0, 1]")
            nodes = [node for node in material.node_tree.nodes if node.type == node_type]
            if len(nodes) != 1 or nodes[0].mute or nodes[0].name not in active:
                raise ValueError("Override requires one unmuted active top-level " + node_type)
            node = nodes[0]
            socket = node.inputs.get(input_name)
            if socket is None or socket.is_linked or material.node_tree.animation_data is not None:
                raise ValueError(input_name + " must be unlinked and not animated")
            before = float(socket.default_value)
            if not math.isfinite(before):
                raise ValueError("Imported " + control + " is nonfinite")
            edits.append((control, node.name, input_name, before, float(value)))
        planned.append((material, edits))
    scene = bpy.context.scene
    raw = scene.get(EVIDENCE_KEY, "[]")
    if not isinstance(raw, str) or len(raw) > 131072:
        raise ValueError("Asset override evidence budget exceeded")
    previous = json.loads(raw)
    if not isinstance(previous, list) or len(previous) + sum(len(edits) for _, edits in planned) > 256:
        raise ValueError("Aggregate asset override evidence budget exceeded")
    copies, bindings, evidence = [], [], []
    try:
        for original, edits in planned:
            copied = original.copy()
            copies.append(copied)
            copied.name = spec["id"] + "__" + original.name
            count = 0
            for mesh in meshes:
                for index, material in enumerate(mesh.materials):
                    if material == original:
                        bindings.append((mesh, index, original))
                        mesh.materials[index] = copied
                        count += 1
            records = []
            for control, node_name, input_name, before, value in edits:
                socket = copied.node_tree.nodes[node_name].inputs[input_name]
                socket.default_value = value
                after = float(socket.default_value)
                if not math.isfinite(after) or not 0 <= after <= 1:
                    raise ValueError("Override produced invalid socket value")
                record = {"asset_id": spec["id"], "source_material": original.name,
                          "material": copied.name, "node": node_name, "control": control,
                          "input": input_name, "before": before, "after": after,
                          "prototype_slot_bindings": count, "status": "APPLIED_NOT_RENDER_QUALIFIED"}
                if control == "normal_strength":
                    record["normal_node"] = node_name
                records.append(record)
            copied[EVIDENCE_KEY] = json.dumps(records[0] if len(records) == 1 else records)
            evidence.extend(records)
        encoded = json.dumps(previous + evidence)
        if len(encoded) > 131072:
            raise ValueError("Asset override evidence byte budget exceeded")
        scene[EVIDENCE_KEY] = encoded
    except Exception:
        for mesh, index, original in reversed(bindings):
            mesh.materials[index] = original
        for copied in copies:
            bpy.data.materials.remove(copied)
        raise
