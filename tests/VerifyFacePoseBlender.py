"""Independently decode source BVH, then verify runtime-derived morph endpoints."""
from pathlib import Path
import json
import math
import sys
import bpy
from mathutils import Quaternion, Vector

directory, assets = map(Path, sys.argv[sys.argv.index("--") + 1:])
source = json.loads((directory / "BvhReference.json").read_text())
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.preferences.addon_enable(module="io_anim_bvh")
assert bpy.ops.import_anim.bvh(filepath=str(assets / "poseunits.bvh"), global_scale=.1,
    frame_start=1, use_fps_scale=False, update_scene_fps=False, rotate_mode="NATIVE",
    axis_forward="Y", axis_up="Z") == {"FINISHED"}
rig = next(obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE")
assert len(rig.data.bones) == 163 and set(rig.data.bones.keys()) == set(source["joints"])
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
rest = {name: rig.pose.bones[name].matrix.to_quaternion().copy() for name in source["joints"]}
conversion = Quaternion((1, 0, 0), math.pi / 2)
def angular_error(a, b):
    dot = abs(sum(x*y for x, y in zip(a, b)))
    norm = math.sqrt(sum(x*x for x in a) * sum(x*x for x in b))
    return 2 * math.acos(min(1., dot / norm))
maximum_rotation_error = 0.
for index, unit in enumerate(source["units"]):
    bpy.context.scene.frame_set(index + 2)
    bpy.context.view_layer.update()
    for name, values in zip(source["joints"], unit["rotations"]):
        x, y, z, w = values
        expected = conversion @ Quaternion((w, x, y, z)) @ conversion.inverted()
        actual = rig.pose.bones[name].matrix.to_quaternion() @ rest[name].inverted()
        error = angular_error(actual, expected)
        maximum_rotation_error = max(maximum_rotation_error, error)
        assert error < 2e-5, (unit["name"], name, error, tuple(actual), tuple(expected))
results = []
convert = lambda p: Vector((p[0], -p[2], p[1]))
for label in ("adult", "child"):
    reference = json.loads((directory / label / "PoseHumanReference.json").read_text())
    for extension, key in (("glb", "renderPoints"), ("fbx", "sourcePoints")):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        filename = str(directory / label / ("PoseHuman." + extension))
        result = bpy.ops.import_scene.gltf(filepath=filename) if extension == "glb" else bpy.ops.import_scene.fbx(filepath=filename)
        assert result == {"FINISHED"}
        meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH" and any(mod.type=="ARMATURE" for mod in obj.modifiers)]
        rigs = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
        assert len(meshes) == 4 and len(rigs) == 1 and len(rigs[0].data.bones) == 163, (label, extension, [(o.name, o.type, len(o.data.bones) if o.type=="ARMATURE" else len(o.data.vertices)) for o in bpy.context.scene.objects])
        for obj in meshes:
            assert obj.data.shape_keys
            for unit in reference["controls"]:
                assert "pose/" + unit in obj.data.shape_keys.key_blocks, (label, extension, obj.name, unit)
            for block in obj.data.shape_keys.key_blocks:
                block.value = 0
        body = next(obj for obj in meshes if obj.name == "OBJ")
        assert "UserDetail" in body.data.shape_keys.key_blocks
        def positions(obj):
            evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
            mesh = evaluated.to_mesh()
            values = [evaluated.matrix_world @ vertex.co for vertex in mesh.vertices]
            evaluated.to_mesh_clear()
            return values
        def check(expected_meshes):
            errors = {}
            for expected in expected_meshes:
                obj = next(obj for obj in meshes if obj.name == expected["name"])
                values = positions(obj)
                assert len(values) == len(expected[key])
                error = max((a-convert(b)).length for a, b in zip(values, expected[key]))
                assert error < 2e-6, (label, extension, obj.name, error)
                errors[obj.name] = error
            return errors
        neutral_errors = check(reference["meshes"])
        unit_errors = {}
        for unit in reference["units"]:
            for obj in meshes:
                obj.data.shape_keys.key_blocks["pose/" + unit["name"]].value = 1
            bpy.context.view_layer.update()
            unit_errors[unit["name"]] = check(unit["meshes"])
            for obj in meshes:
                obj.data.shape_keys.key_blocks["pose/" + unit["name"]].value = 0
            bpy.context.view_layer.update()
        bpy.ops.wm.save_as_mainfile(filepath=str(directory / label / ("PoseHuman-" + extension + ".blend")))
        results.append({"shape": label, "format": extension, "units": len(reference["controls"]),
            "meshes": 4, "bones": 163, "neutralPositionErrorsMetres": neutral_errors,
            "unitPositionErrorsMetres": unit_errors, "customMorphPreserved": True})
report = {"blender": bpy.app.version_string, "sourceBvhRotationComparisons": 59*163,
          "sourceBvhMaximumAngleErrorRadians": maximum_rotation_error, "results": results}
(directory / "BlenderFacePoseVerification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))
