"""Headless Blender renderer: glTF models -> sprites in OpenTTD's projection.

Run by noirgfx, never imported:  blender -b --factory-startup -P blender_render.py -- jobs.json

Camera: orthographic, looking from the south corner down at 30 degrees, so a tile (1 x 1 Blender
units, OpenTTD map x along Blender +X, map y along +Y) is a 2:1 rhombus 64 px wide at 1x. Each job
writes two PNGs: the lit colour image and a world-normal image (normal * 0.5 + 0.5), both RGBA on a
square canvas centred on the world origin, which is where the sprite is anchored in the game.

Job keys: glb, out (path prefix), zoom, target_h (px of the model's screen height, or null),
scale (multiplier after fitting), snow (bool: white material on upward faces), dark (0..1: darken
leaves), seed.
"""

from __future__ import annotations

import json
import math
import sys

import bpy
from mathutils import Vector

PPU_1X = 64 / math.sqrt(2)  # screen px per Blender unit at 1x
BACK = Vector((math.sqrt(1.5) / 2, math.sqrt(1.5) / 2, 0.5)).normalized()  # camera -> scene is -BACK
RIGHT = Vector((-1, 1, 0)).normalized()
UP = BACK.cross(RIGHT).normalized()


def reset() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.render.filter_size = 0.75
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    try:
        sc.eevee.taa_render_samples = 16
    except AttributeError:
        pass
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (1, 1, 1, 1)
    bg.inputs["Strength"].default_value = 0.35
    sc.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.2
    sun.data.angle = math.radians(8)
    # Light from the upper left of the screen, as OpenTTD's sprites are lit.
    sun.rotation_euler = Vector((0.55, 0.15, 0.82)).normalized().to_track_quat("Z", "Y").to_euler()
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    cam.location = BACK * 50
    cam.rotation_euler = (-BACK).to_track_quat("-Z", "Y").to_euler()
    cam.data.clip_start, cam.data.clip_end = 0.1, 200
    sc.collection.objects.link(cam)
    sc.camera = cam


def import_model(path: str) -> list[bpy.types.Object]:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    objs = [o for o in bpy.data.objects if o not in before]
    roots = [o for o in objs if o.parent is None]
    group = bpy.data.objects.new("model", None)
    bpy.context.scene.collection.objects.link(group)
    for r in roots:
        r.parent = group
    return [group] + objs


def bounds(objs: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in objs if o.type == "MESH" for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def screen_extent(lo: Vector, hi: Vector) -> tuple[float, float, float, float]:
    corners = [Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
    us = [c.dot(RIGHT) for c in corners]
    vs = [-c.dot(UP) for c in corners]
    return min(us), max(us), min(vs), max(vs)


def place(objs: list[bpy.types.Object], job: dict) -> float:
    """Centre the footprint on the origin, base at z = 0, scale to the target screen height."""
    group = objs[0]
    lo, hi = bounds(objs)
    group.location = Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))
    lo, hi = bounds(objs)
    s = job.get("scale", 1.0)
    if job.get("target_h"):
        _, _, v0, v1 = screen_extent(lo, hi)
        s *= job["target_h"] / ((v1 - v0) * PPU_1X * job["zoom"])
    elif job.get("footprint"):
        s *= job["footprint"] / max(hi.x - lo.x, hi.y - lo.y)
    group.scale = (s, s, s)
    group.location *= s
    return s


def material_overrides(objs: list[bpy.types.Object], job: dict) -> None:
    if not (job.get("snow") or job.get("dark")):
        return
    for o in objs:
        if o.type != "MESH":
            continue
        for slot in o.material_slots:
            m = slot.material
            if m is None or not m.use_nodes:
                continue
            nt = m.node_tree
            bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
            if bsdf is None:
                continue
            src = bsdf.inputs["Base Color"]
            colour = nt.nodes.new("ShaderNodeRGB")
            colour.outputs[0].default_value = tuple(bsdf.inputs["Base Color"].default_value)
            base = src.links[0].from_socket if src.links else colour.outputs[0]
            if job.get("dark"):
                mix = nt.nodes.new("ShaderNodeMix")
                mix.data_type = "RGBA"
                mix.inputs["Factor"].default_value = job["dark"]
                nt.links.new(base, mix.inputs[6])
                mix.inputs[7].default_value = (0.05, 0.04, 0.03, 1)
                base = mix.outputs[2]
            if job.get("snow"):
                geo = nt.nodes.new("ShaderNodeNewGeometry")
                sep = nt.nodes.new("ShaderNodeSeparateXYZ")
                ramp = nt.nodes.new("ShaderNodeMapRange")
                ramp.inputs["From Min"].default_value = 0.15
                ramp.inputs["From Max"].default_value = 0.3
                nt.links.new(geo.outputs["Normal"], sep.inputs[0])
                nt.links.new(sep.outputs["Z"], ramp.inputs["Value"])
                mix = nt.nodes.new("ShaderNodeMix")
                mix.data_type = "RGBA"
                nt.links.new(ramp.outputs["Result"], mix.inputs["Factor"])
                nt.links.new(base, mix.inputs[6])
                mix.inputs[7].default_value = (0.95, 0.95, 0.97, 1)
                base = mix.outputs[2]
            nt.links.new(base, src)


def normal_material() -> bpy.types.Material:
    m = bpy.data.materials.get("noir_normal")
    if m:
        return m
    m = bpy.data.materials.new("noir_normal")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    mul = nt.nodes.new("ShaderNodeVectorMath")
    mul.operation = "MULTIPLY_ADD"
    mul.inputs[1].default_value = (0.5, 0.5, 0.5)
    mul.inputs[2].default_value = (0.5, 0.5, 0.5)
    emit = nt.nodes.new("ShaderNodeEmission")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(geo.outputs["Normal"], mul.inputs[0])
    nt.links.new(mul.outputs[0], emit.inputs["Color"])
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    return m


def render(job: dict) -> dict:
    reset()
    objs = import_model(job["glb"])
    material_overrides(objs, job)
    s = place(objs, job)
    lo, hi = bounds(objs)
    u0, u1, v0, v1 = screen_extent(lo, hi)
    ppu = PPU_1X * job["zoom"]
    half = int(math.ceil(max(abs(u0), abs(u1), abs(v0), abs(v1)) * ppu)) + 4
    sc = bpy.context.scene
    sc.render.resolution_x = sc.render.resolution_y = 2 * half
    sc.camera.data.ortho_scale = 2 * half / ppu
    sc.render.filepath = job["out"] + "_rgb.png"
    bpy.ops.render.render(write_still=True)
    # Normals: every mesh gets the emission-normal material, with no lights or world.
    nm = normal_material()
    for o in objs:
        if o.type == "MESH":
            o.data.materials.clear()
            o.data.materials.append(nm)
    sc.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0
    sc.render.filter_size = 0.01
    sc.render.filepath = job["out"] + "_nrm.png"
    bpy.ops.render.render(write_still=True)
    return {"out": job["out"], "origin": [half, half], "scale": s}


def main() -> None:
    jobs = json.load(open(sys.argv[sys.argv.index("--") + 1]))
    results = [render(j) for j in jobs]
    json.dump(results, open(sys.argv[sys.argv.index("--") + 2], "w"), indent=1)


main()
