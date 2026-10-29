"""Verify actual shape-control output against original C++ points and joint heads."""
from pathlib import Path
import json
import sys
import bpy
from mathutils import Vector, Quaternion

directory = Path(sys.argv[sys.argv.index("--") + 1])
reference = json.loads((directory / "ControlledHumanReference.json").read_text())
results = []
convert = lambda p: Vector((p[0], -p[2], p[1]))
for extension, key in (("glb", "renderPoints"), ("fbx", "sourcePoints")):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = str(directory / ("ControlledHuman." + extension))
    result = bpy.ops.import_scene.gltf(filepath=path) if extension == "glb" else bpy.ops.import_scene.fbx(filepath=path)
    assert result == {"FINISHED"}
    rigs = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"
              and any(modifier.type == "ARMATURE" for modifier in obj.modifiers)]
    assert len(rigs) == 1 and len(meshes) == 1, (extension, [(obj.name, obj.type) for obj in bpy.context.scene.objects])
    rig, mesh = rigs[0], meshes[0]
    assert len(rig.data.bones) == 163 and len(mesh.data.vertices) == len(reference[key])
    depsgraph = bpy.context.evaluated_depsgraph_get()
    def positions():
        evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        temporary = evaluated.to_mesh()
        result = [evaluated.matrix_world @ vertex.co for vertex in temporary.vertices]
        evaluated.to_mesh_clear()
        return result
    rest = positions()
    position_error = max((actual - convert(expected)).length for actual, expected in zip(rest, reference[key]))
    joint_error = max((rig.matrix_world @ rig.data.bones[name].head_local - convert(point)).length
                      for name, point in reference["joints"].items())
    assert position_error < 2e-6 and joint_error < 2e-6, (extension, position_error, joint_error)
    elbow = rig.pose.bones["lowerarm01.L"]
    elbow.rotation_mode = "QUATERNION"
    elbow.rotation_quaternion = elbow.rotation_quaternion @ Quaternion((0, 0, 1), .5)
    bpy.context.view_layer.update()
    movement = max((new - old).length for new, old in zip(positions(), rest))
    assert movement > .03
    results.append({"format": extension, "vertices": len(rest), "bones": 163,
                    "sourcePositionErrorMetres": position_error, "sourceJointErrorMetres": joint_error,
                    "skinMovementMetres": movement})
    bpy.ops.wm.save_as_mainfile(filepath=str(directory / ("ControlledHuman-" + extension + ".blend")))
report = {"blender": bpy.app.version_string, "controls": 250, "results": results}
(directory / "BlenderTemplateControlsVerification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))
