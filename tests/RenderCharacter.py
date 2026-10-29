"""생성 메시의 외형을 확인하기 위한 Blender 검증 도구이다. 엔진 의존성이 아니다."""
from pathlib import Path
import sys

import bpy
from mathutils import Vector


source, destination = map(Path, sys.argv[sys.argv.index("--") + 1:])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(source))
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 16
scene.render.resolution_x = 900
scene.render.resolution_y = 960
scene.render.resolution_percentage = 100
scene.world = bpy.data.worlds.new("ReviewWorld")
scene.world.use_nodes = True
scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (.1, .1, .1, 1)
scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = .6
for name, location, energy, size in (
    ("Key", (-2, -3, 4), 350, 3),
    ("Fill", (2, -1, 2), 170, 2),
    ("Rim", (0, 2, 3), 250, 2),
):
    light = bpy.data.lights.new(name, "AREA")
    light.energy = energy
    light.shape = "DISK"
    light.size = size
    obj = bpy.data.objects.new(name, light)
    scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (Vector((0, 0, 1)) - obj.location).to_track_quat("-Z", "Y").to_euler()
camera = bpy.data.cameras.new("ReviewCamera")
obj = bpy.data.objects.new("ReviewCamera", camera)
scene.collection.objects.link(obj)
obj.location = (0, -3.2, 1.0)
obj.rotation_euler = (Vector((0, 0, .92)) - obj.location).to_track_quat("-Z", "Y").to_euler()
camera.type = "ORTHO"
camera.ortho_scale = 2.05
scene.camera = obj
destination.parent.mkdir(parents=True, exist_ok=True)
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(destination)
bpy.ops.render.render(write_still=True)
