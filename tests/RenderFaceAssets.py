"""Render source face assets for visual review; this is an optional host tool."""
from pathlib import Path
import sys
import bpy
from mathutils import Vector, Quaternion

source, destination = map(Path, sys.argv[sys.argv.index("--") + 1:])
bpy.ops.wm.read_factory_settings(use_empty=True)
assert bpy.ops.import_scene.gltf(filepath=str(source)) == {"FINISHED"}
rig = next(obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE")
eyes = [rig.matrix_world @ rig.data.bones[name].head_local for name in ("eye.L", "eye.R")]
centre = (eyes[0] + eyes[1]) * .5 + Vector((0, 0, -.04))
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 24
scene.render.resolution_x = scene.render.resolution_y = 800
scene.render.resolution_percentage = 100
scene.world = bpy.data.worlds.new("FaceReviewWorld")
scene.world.use_nodes = True
scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = .3
for name, offset, power, size in (("Key", (-1, -2, 1.5), 130, 1.6),
                                  ("Fill", (1.5, -1, .2), 80, 1.8)):
    data = bpy.data.lights.new(name, "AREA")
    data.energy, data.size = power, size
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = centre + Vector(offset)
    obj.rotation_euler = (centre - obj.location).to_track_quat("-Z", "Y").to_euler()
camera = bpy.data.cameras.new("FaceReviewCamera")
obj = bpy.data.objects.new("FaceReviewCamera", camera)
scene.collection.objects.link(obj)
obj.location = centre + Vector((0, -1.5, 0))
obj.rotation_euler = (centre - obj.location).to_track_quat("-Z", "Y").to_euler()
camera.type, camera.ortho_scale = "ORTHO", .5
scene.camera = obj
destination.mkdir(parents=True, exist_ok=True)
scene.render.image_settings.file_format = "PNG"
bone = rig.pose.bones["jaw"]
bone.rotation_mode = "QUATERNION"
rest = bone.rotation_quaternion.copy()
axis = bone.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0))
for label, angle in (("Neutral", 0), ("Jaw", .4)):
    bone.rotation_quaternion = rest @ Quaternion(axis.normalized(), angle)
    bpy.context.view_layer.update()
    scene.render.filepath = str(destination / (label + ".png"))
    bpy.ops.render.render(write_still=True)
