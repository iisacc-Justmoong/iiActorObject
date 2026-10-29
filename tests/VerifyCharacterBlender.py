"""실제 Blender에서 가져오기, 변형, 편집 및 재저장한다. 검증 전용 보조 코드이다."""
import json
import hashlib
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector


def close(a, b, epsilon=2e-5):
    assert (Vector(a) - Vector(b)).length < epsilon, (tuple(a), tuple(b))


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    operation = bpy.ops.import_scene.fbx if path.suffix == ".fbx" else bpy.ops.import_scene.gltf
    assert operation(filepath=str(path)) == {"FINISHED"}
    armatures = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    assert len(armatures) == 1
    armature = armatures[0]
    # glTF importer creates mesh widgets for pose-bone custom shapes as scene objects.
    widgets = {p.custom_shape for p in armature.pose.bones if p.custom_shape is not None}
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and o not in widgets]
    for obj in meshes:
        assert any(m.type == "ARMATURE" and m.object == armature for m in obj.modifiers)
    return armature, meshes


def evaluated(obj):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    mesh = obj.evaluated_get(depsgraph).to_mesh()
    points = [obj.matrix_world @ v.co.copy() for v in mesh.vertices]
    obj.evaluated_get(depsgraph).to_mesh_clear()
    return points


def main():
    fixture, generated, fbx_fixture, fbx_generated, influence_fixture, static_glb, static_fbx, directory = map(Path, sys.argv[sys.argv.index("--") + 1:])
    directory.mkdir(parents=True, exist_ok=True)
    report = directory / "blender-verification.json"
    report.unlink(missing_ok=True)
    for path in (static_glb, static_fbx):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        operation = bpy.ops.import_scene.fbx if path.suffix == ".fbx" else bpy.ops.import_scene.gltf
        assert operation(filepath=str(path)) == {"FINISHED"}
        assert not any(o.type == "ARMATURE" for o in bpy.context.scene.objects)
        static_meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        assert len(static_meshes) == 1 and len(static_meshes[0].data.vertices) > 100
    for path in (fixture, fbx_fixture):
        armature, meshes = load(path)
        assert set(armature.data.bones.keys()) == {"root", "pelvis", "head"}
        body = meshes[0]
        assert len(body.data.vertices) == 4
        close(evaluated(body)[2], (1, 0, 1))
        body.data.shape_keys.key_blocks["smile"].value = .5
        bpy.context.view_layer.update()
        pose = armature.pose.bones["head"]
        # FBX can import a connected head bone: its pose location is intentionally locked
        # to the parent tail. Rotate about the joint in world space in both formats.
        pivot = armature.matrix_world @ pose.matrix.translation
        delta = Matrix.Translation(pivot) @ Matrix.Rotation(.3, 4, "Y") @ Matrix.Translation(-pivot)
        pose.matrix = armature.matrix_world.inverted() @ delta @ armature.matrix_world @ pose.matrix
        bpy.context.view_layer.update()
        close(evaluated(body)[2], delta @ Vector((1, -.25, 1)))
        close(evaluated(body)[0], (-1, 0, -1))

    armature, meshes = load(influence_fixture)
    body = meshes[0]
    assert len(armature.data.bones) == 13
    assert all(len(v.groups) == 12 for v in body.data.vertices)
    bpy.context.view_layer.update()
    joint = armature.pose.bones["joint_12"]
    matrix = joint.matrix.copy()
    matrix.translation += armature.matrix_world.to_3x3().inverted() @ Vector((1, 0, 0))
    joint.matrix = matrix
    bpy.context.view_layer.update()
    close(evaluated(body)[0], (-1 + 1 / 12, 0, -1))

    armature, meshes = load(generated)
    body = next(o for o in meshes if o.name == "Body")
    assert len(meshes) == 3 and len(armature.data.bones) == 71
    assert len(body.data.shape_keys.key_blocks) == 53  # Basis and 52 semantic channels.
    neutral = evaluated(body)
    for vertex in body.data.vertices:
        assert 1 <= len(vertex.groups) <= 4
        assert abs(sum(g.weight for g in vertex.groups) - 1) < 2e-5
    body.data.shape_keys.key_blocks["mouthSmileLeft"].value = .8
    bpy.context.view_layer.update()
    smiling = evaluated(body)
    smile_displacement = max((a - b).length for a, b in zip(neutral, smiling))
    assert smile_displacement > .001
    upper = armature.pose.bones["upperarm_l"]
    upper.rotation_mode = "XYZ"
    upper.rotation_euler.y = .3
    bpy.context.view_layer.update()
    posed = evaluated(body)
    skin_displacement = max((a - b).length for a, b in zip(smiling, posed))
    assert skin_displacement > .02
    # Detailed mesh editing and re-export retain the skin and named morphs.
    body.data.vertices[0].co.z += .001
    bpy.ops.object.select_all(action="DESELECT")
    for obj in [armature] + meshes:
        obj.select_set(True)
    assert bpy.ops.export_scene.fbx(filepath=str(directory / "GeneratedHuman-roundtrip.fbx"), use_selection=True,
                                    add_leaf_bones=False, bake_anim=False, axis_forward="-Z", axis_up="Y") == {"FINISHED"}
    roundtrip = directory / "GeneratedHuman-roundtrip.glb"
    assert bpy.ops.export_scene.gltf(filepath=str(roundtrip), export_format="GLB", export_animations=False,
                                     export_all_influences=True) == {"FINISHED"}
    bpy.ops.wm.save_as_mainfile(filepath=str(directory / "GeneratedHuman.blend"))
    bone_names = sorted(armature.data.bones.keys())
    armature, meshes = load(roundtrip)
    body = next(o for o in meshes if o.name == "Body")
    assert sorted(armature.data.bones.keys()) == bone_names
    assert "mouthSmileLeft" in body.data.shape_keys.key_blocks
    roundtrip_vertices = len(body.data.vertices)
    roundtrip_meshes = len(meshes)
    fbx_armature, fbx_meshes = load(fbx_generated)
    assert sorted(fbx_armature.data.bones.keys()) == bone_names
    fbx_body = next(o for o in fbx_meshes if o.name == "Body")
    assert len(fbx_meshes) == 3 and len(fbx_body.data.shape_keys.key_blocks) == 53
    result = {"blender": bpy.app.version_string, "bones": len(bone_names), "meshParts": roundtrip_meshes,
              "bodyVerticesAfterRoundtrip": roundtrip_vertices, "smileDisplacementMetres": smile_displacement,
              "skinDisplacementMetres": skin_displacement, "fixtureSkinAndMorphReference": True,
              "editableRoundtrip": True, "fbxImported": True, "fbxSkinAndMorphReference": True,
              "twelveInfluencesPreserved": True,
              "coreStaticObjectsWithoutArmature": True,
              "inputSha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (fixture, generated, fbx_fixture, fbx_generated, influence_fixture, static_glb, static_fbx)}}
    report.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


main()
