"""Bounded, content-pinned HDR environments and portable render-state restoration."""
import bpy
import hashlib
import math
from pathlib import Path

MAX_BYTES = 64 * 1024 * 1024
SOURCE_HASH = "singular_environment_sha256"


def packed_bytes(image):
    if image.packed_file is None or not 0 < image.packed_file.size <= MAX_BYTES:
        raise ValueError("Environment image must be packed and within the byte budget")
    return bytes(image.packed_file.data)


def configure_world(spec, digest):
    strength = spec["strength"]
    color = spec["color"]
    if (type(strength) not in (int, float) or not math.isfinite(strength)
            or not 0 <= strength <= 1000 or not isinstance(color, list) or len(color) not in (3, 4)
            or any(type(v) not in (int, float) or not math.isfinite(v) or not 0 <= v <= 1 for v in color)):
        raise ValueError("World radiance parameters outside bounds")
    world = bpy.data.worlds.new("Nocturne environment")
    world.use_nodes = True
    background = world.node_tree.nodes["Background"]
    background.inputs["Color"].default_value = (*color[:3], 1)
    background.inputs["Strength"].default_value = strength
    environment = spec.get("environment")
    if environment is not None:
        if not isinstance(environment, dict) or set(environment) != {"uri", "sha256"}:
            raise ValueError("Environment requires exactly uri and sha256")
        uri, expected = environment["uri"], environment["sha256"]
        if (not isinstance(uri, str) or len(uri) > 4096 or not Path(uri).is_absolute()
                or not isinstance(expected, str) or len(expected) != 64
                or any(c not in "0123456789abcdef" for c in expected)):
            raise ValueError("Invalid environment path or SHA256")
        path = Path(uri).resolve()
        if path.suffix.lower() != ".exr" or not path.is_file() or not 0 < path.stat().st_size <= MAX_BYTES:
            raise ValueError("Environment image missing, unsupported or over budget")
        if digest(path) != expected:
            raise ValueError("Environment image SHA256 mismatch")
        image = bpy.data.images.load(str(path), check_existing=False)
        width, height = image.size
        if not image.is_float or not 0 < width <= 4096 or not 0 < height <= 2048 or width != 2 * height:
            raise ValueError("Environment requires a bounded floating-point 2:1 panorama")
        image.colorspace_settings.name = "Linear Rec.709"
        image.pack()
        if hashlib.sha256(packed_bytes(image)).hexdigest() != expected:
            raise ValueError("Packed environment bytes changed during import")
        image[SOURCE_HASH] = expected
        texture = world.node_tree.nodes.new("ShaderNodeTexEnvironment")
        texture.name = "Nocturne HDR Environment"
        texture.image = image
        texture.projection = "EQUIRECTANGULAR"
        texture.interpolation = "Linear"
        world.node_tree.links.new(texture.outputs["Color"], background.inputs["Color"])
    bpy.context.scene.world = world
    return world


def world_background(world):
    if world is None:
        return None
    if not world.use_nodes or world.node_tree.animation_data is not None or world.animation_data is not None:
        raise ValueError("Only static node-based worlds have portable render state")
    nodes = world.node_tree.nodes
    background = nodes.get("Background")
    outputs = [node for node in nodes if node.type == "OUTPUT_WORLD" and node.is_active_output]
    if (background is None or background.type != "BACKGROUND" or background.mute
            or len(outputs) != 1 or outputs[0].mute or outputs[0].inputs["Volume"].is_linked
            or background.inputs["Strength"].is_linked or len(outputs[0].inputs["Surface"].links) != 1
            or outputs[0].inputs["Surface"].links[0].from_node != background):
        raise ValueError("Unsupported active world graph cannot be exported as constant or HDR lighting")
    expected_nodes = 3 if background.inputs["Color"].is_linked else 2
    if len(nodes) != expected_nodes or len(world.node_tree.links) != expected_nodes - 1:
        raise ValueError("World graph exceeds the supported constant or single-HDR topology")
    return background


def environment_state(world, output=None):
    background = world_background(world)
    if background is None or not background.inputs["Color"].is_linked:
        return None
    color_links = list(background.inputs["Color"].links)
    texture = color_links[0].from_node
    if (texture.mute or texture.type != "TEX_ENVIRONMENT"
            or color_links[0].from_socket.name != "Color" or background.inputs["Strength"].is_linked
            or texture.inputs["Vector"].is_linked or texture.projection != "EQUIRECTANGULAR"
            or texture.interpolation != "Linear" or texture.image is None
            or tuple(texture.texture_mapping.rotation) != (0, 0, 0)
            or tuple(texture.texture_mapping.translation) != (0, 0, 0)
            or tuple(texture.texture_mapping.scale) != (1, 1, 1)):
        raise ValueError("Environment graph differs from the supported unrotated HDR setup")
    image = texture.image
    payload = packed_bytes(image)
    observed = hashlib.sha256(payload).hexdigest()
    if observed != image.get(SOURCE_HASH) or image.colorspace_settings.name != "Linear Rec.709":
        raise ValueError("Environment packed hash or color space mismatch")
    suffix = Path(image.filepath).suffix.lower()
    if suffix != ".exr":
        raise ValueError("Unsupported packed environment format")
    relative = "environment/" + observed + suffix
    if output is not None:
        path = Path(output) / relative
        path.parent.mkdir(exist_ok=True)
        with path.open("xb") as stream:
            stream.write(payload)
    return {"schema": "singular.world_environment/1", "path": relative, "sha256": observed,
            "bytes": len(payload), "dimensions": list(image.size), "packed": True,
            "projection": texture.projection, "color_space": image.colorspace_settings.name,
            "strength": background.inputs["Strength"].default_value,
            "scope": "Distant image-based radiance; not local geometry or calibrated illuminance"}


def restore_environment(state, directory, digest):
    if state.get("schema") != "singular.world_environment/1":
        raise ValueError("Unsupported environment state")
    root = Path(directory).resolve()
    path = (root / state["path"]).resolve()
    if path.parent != root / "environment" or path.stem != state["sha256"]:
        raise ValueError("Environment package path must be content-addressed and local")
    configure_world({"color": [0, 0, 0], "strength": state["strength"],
                     "environment": {"uri": str(path), "sha256": state["sha256"]}}, digest)
    observed = environment_state(bpy.context.scene.world)
    if any(observed[key] != state[key] for key in ("bytes", "dimensions", "projection", "color_space", "strength")):
        raise ValueError("Restored environment differs from packaged state")
