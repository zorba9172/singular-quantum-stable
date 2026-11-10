"""Bounded observations of assigned material graphs; linked shader values stay unknown."""
import math
import bpy

MAX_MATERIALS = 256
MAX_NODES = 128
MAX_LINKS = 512
MAX_CAR_OBJECTS = 256


def finite_value(value):
    if isinstance(value, (bool, int, float)):
        if not math.isfinite(value):
            raise ValueError("Nonfinite material socket value")
        return value
    if hasattr(value, "__len__") and not isinstance(value, str) and 1 <= len(value) <= 4:
        values = list(value)
        if all(isinstance(v, (bool, int, float)) and math.isfinite(v) for v in values):
            return values
    return None


def link_observation(link):
    return {"from_node": link.from_node.name, "from_socket": link.from_socket.identifier,
            "to_node": link.to_node.name, "to_socket": link.to_socket.identifier,
            "valid": link.is_valid, "muted": link.is_muted}


def input_observation(socket):
    value = {"socket": socket.name, "identifier": socket.identifier, "type": socket.type}
    if socket.is_linked:
        if len(socket.links) > 16:
            raise ValueError("Material socket link budget exceeded")
        value.update({"value_status": "LINKED_UNEVALUATED",
                      "sources": [link_observation(link) for link in socket.links]})
    else:
        observed = finite_value(getattr(socket, "default_value", None))
        value.update({"value_status": "UNLINKED_CONSTANT" if observed is not None else "UNAVAILABLE",
                      "value": observed})
    return value


def surface_nodes(nodes):
    outputs = [node for node in nodes if node.type == "OUTPUT_MATERIAL" and node.is_active_output]
    seen, pending = set(), []
    for node in outputs:
        socket = node.inputs.get("Surface")
        if socket:
            pending.extend(link.from_node for link in socket.links if link.is_valid and not link.is_muted)
    while pending:
        node = pending.pop()
        if node.name in seen:
            continue
        seen.add(node.name)
        if len(seen) > MAX_NODES:
            raise ValueError("Material traversal budget exceeded")
        for socket in node.inputs:
            pending.extend(link.from_node for link in socket.links if link.is_valid and not link.is_muted)
    return seen


def observe_material(mat, binding_count, budget):
    result = {"material": mat.name, "assigned_object_count": binding_count,
              "use_nodes": mat.use_nodes, "nodes": [], "links": [], "surface_principled": []}
    if not mat.use_nodes or mat.node_tree is None:
        result["status"] = "NO_NODE_GRAPH"
        return result
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    if len(nodes) > MAX_NODES or len(links) > MAX_LINKS:
        raise ValueError("Material graph budget exceeded: " + mat.name)
    budget[0] += len(nodes)
    budget[1] += len(links)
    if budget[0] > 8192 or budget[1] > 16384:
        raise ValueError("Aggregate material graph budget exceeded")
    active = surface_nodes(nodes)
    for node in sorted(nodes, key=lambda item: item.name):
        if len(node.inputs) > 64:
            raise ValueError("Material node input budget exceeded")
        budget[2] += len(node.inputs)
        if budget[2] > 32768:
            raise ValueError("Aggregate material socket budget exceeded")
        item = {"node": node.name, "type": node.bl_idname, "muted": node.mute,
                "surface_reachable": node.name in active,
                "inputs": [input_observation(socket) for socket in node.inputs]}
        if node.type == "BSDF_PRINCIPLED" and node.name in active:
            result["surface_principled"].append(node.name)
        if node.type == "OUTPUT_MATERIAL":
            item.update({"active_output": node.is_active_output, "target": node.target})
        if node.type == "GROUP":
            item["group_tree"] = node.node_tree.name if node.node_tree else None
            item["group_contents"] = "NOT_EXPANDED"
        if node.type == "TEX_IMAGE":
            image = node.image
            if image and len(image.filepath) > 4096:
                raise ValueError("Material image path budget exceeded")
            item["image"] = None if image is None else {
                "name": image.name, "filepath": image.filepath, "source": image.source,
                "size": list(image.size), "colorspace": image.colorspace_settings.name,
                "packed": bool(image.packed_file), "has_data": image.has_data}
        if node.type == "VALTORGB":
            if len(node.color_ramp.elements) > 32:
                raise ValueError("Material color ramp budget exceeded")
            item["color_ramp"] = {"interpolation": node.color_ramp.interpolation,
                "elements": [{"position": element.position, "color": finite_value(element.color)}
                             for element in node.color_ramp.elements]}
        result["nodes"].append(item)
    result["links"] = [link_observation(link) for link in links]
    result["status"] = "NODE_GRAPH_OBSERVED"
    return result


def inspect_materials():
    scene = bpy.context.scene
    if len(scene.objects) > 10000:
        raise ValueError("Material inspection object budget exceeded")
    assigned, counts, cars = {}, {}, []
    car_count = 0
    for obj in sorted(scene.objects, key=lambda item: item.name):
        if obj.type != "MESH":
            continue
        if len(obj.material_slots) > 64:
            raise ValueError("Object material slot budget exceeded")
        slots = [{"slot": index, "material": slot.material.name if slot.material else None,
                  "binding": slot.link} for index, slot in enumerate(obj.material_slots)]
        for name in {slot["material"] for slot in slots if slot["material"] is not None}:
            assigned[name] = bpy.data.materials[name]
            counts[name] = counts.get(name, 0) + 1
        if len(assigned) > MAX_MATERIALS:
            raise ValueError("Assigned material budget exceeded")
        if obj.name.lower().startswith(("specter_", "car_", "vehicle_")):
            car_count += 1
            if len(cars) < MAX_CAR_OBJECTS:
                cars.append({"object": obj.name, "mesh": obj.data.name,
                             "hide_render": obj.hide_render, "material_slots": slots})
    budget = [0, 0, 0]
    return {"status": "ASSIGNED_MATERIAL_GRAPHS_OBSERVED", "frame": scene.frame_current,
        "materials": [observe_material(assigned[name], counts[name], budget) for name in sorted(assigned)],
        "car_object_bindings": cars, "car_objects_matching": car_count,
        "car_binding_sample_limit": MAX_CAR_OBJECTS, "car_bindings_truncated": car_count > len(cars),
        "car_selection": "Mesh object names beginning specter_, car_, or vehicle_",
        "limits": {"materials": MAX_MATERIALS, "nodes_per_material": MAX_NODES,
                   "links_per_material": MAX_LINKS, "total_nodes": 8192, "total_sockets": 32768},
        "limitations": ["Assigned slots include hidden geometry and unused polygon slots; not pixel contribution",
            "Linked inputs are unevaluated, never reported as their fallback constants",
            "Surface reachability follows top-level active material outputs; node groups are not expanded",
            "Graph inspection does not establish rendered noise, sampling convergence or visual equivalence"]}
