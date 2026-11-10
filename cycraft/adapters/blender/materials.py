"""Blender API adapter only. Artistic parameters come from the native scene contract."""
import bpy
import math
from pathlib import Path

def texture_material(mat, bsdf, spec):
    tile = spec["tile_size_m"]
    if not isinstance(tile, (float, int)) or not math.isfinite(tile) or not 0.01 <= tile <= 100:
        raise ValueError("PBR tile size is outside metric bounds")
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    coords = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (1 / tile,) * 3
    links.new(coords.outputs["UV"], mapping.inputs["Vector"])
    for role, socket in (("base_color", "Base Color"), ("roughness", "Roughness"), ("normal", "Normal")):
        path = Path(spec[role]).resolve()
        if not path.is_file() or path.stat().st_size > 16 * 1024 * 1024:
            raise ValueError(f"PBR texture missing or over budget: {path}")
        image = bpy.data.images.load(str(path), check_existing=True)
        if max(image.size) > 4096 or min(image.size) <= 0:
            raise ValueError("PBR texture resolution exceeds budget")
        image.colorspace_settings.name = "sRGB" if role == "base_color" else "Non-Color"
        node = nodes.new("ShaderNodeTexImage")
        node.image = image
        node.extension = "REPEAT"
        links.new(mapping.outputs["Vector"], node.inputs["Vector"])
        output = node.outputs["Color"]
        if role == "normal":
            normal = nodes.new("ShaderNodeNormalMap")
            normal.space = "TANGENT"
            links.new(output, normal.inputs["Color"])
            output = normal.outputs["Normal"]
        links.new(output, bsdf.inputs[socket])

def make_material(spec):
    mat = bpy.data.materials.new(spec["id"])
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    color = (*spec["color"], 1)
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Roughness"].default_value = spec["roughness"]
    bsdf.inputs["Metallic"].default_value = spec["metallic"]
    transmission = spec.get("transmission", 0)
    if type(transmission) not in (int, float) or not math.isfinite(transmission) or not 0 <= transmission <= 1:
        raise ValueError("Material transmission must be in 0..1")
    bsdf.inputs["Transmission Weight"].default_value = transmission
    bsdf.inputs["Emission Color"].default_value = color
    bsdf.inputs["Emission Strength"].default_value = spec["emission_strength"]
    if "textures" in spec:
        texture_material(mat, bsdf, spec["textures"])
        return mat
    surface = spec["surface"]
    if surface != "smooth":
        coords = nodes.new("ShaderNodeTexCoord")
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = {"asphalt": 110, "stone": 30, "fabric": 190}[surface]
        noise.inputs["Detail"].default_value = 3
        links.new(coords.outputs["Object"], noise.inputs["Vector"])
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.22
        bump.inputs["Distance"].default_value = {"asphalt": 0.012, "stone": 0.008, "fabric": 0.0003}[surface]
        links.new(noise.outputs["Fac"], bump.inputs["Height"])
        links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        ramp = nodes.new("ShaderNodeValToRGB")
        low = max(0.035, spec["roughness"] * 0.62)
        high = min(1, spec["roughness"] * 1.25)
        ramp.color_ramp.elements[0].color = (low, low, low, 1)
        ramp.color_ramp.elements[1].color = (high, high, high, 1)
        links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        links.new(ramp.outputs["Color"], bsdf.inputs["Roughness"])
    return mat

