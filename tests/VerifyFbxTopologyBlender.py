"""실제 Blender에서 제어점·사각형·머티리얼·스킨·모프 교환을 검사한다."""
from pathlib import Path
import json
import sys
import bpy
from mathutils import Quaternion

directory = Path(sys.argv[sys.argv.index("--") + 1])
results = []
for name in ("Authored.fbx", "Materials.fbx", "Materials.glb"):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = directory / name
    if path.suffix == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(path))
    else:
        bpy.ops.import_scene.gltf(filepath=str(path))
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"
              and any(m.type == "ARMATURE" for m in o.modifiers)]
    assert len(meshes) == 1, name
    mesh = meshes[0]
    rigs = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    assert len(rigs) == 1 and len(rigs[0].data.bones) == 2, name
    if path.suffix == ".fbx":
        assert len(mesh.data.vertices) == 9 and len(mesh.data.polygons) == 2, name
        assert all(len(p.vertices) == 4 for p in mesh.data.polygons), name
    mesh.data.calc_loop_triangles()
    assert len(mesh.data.loop_triangles) == 4, name
    if name.startswith("Materials"):
        assert len({p.material_index for p in mesh.data.polygons}) == 2, name
    keys = mesh.data.shape_keys.key_blocks
    assert "Smile" in keys and "Hidden" in keys, name
    graph = bpy.context.evaluated_depsgraph_get()
    def points():
        obj = mesh.evaluated_get(graph)
        data = obj.to_mesh()
        result = [obj.matrix_world @ v.co for v in data.vertices]
        obj.to_mesh_clear()
        return result
    rest = points()
    keys["Smile"].value = 1
    bpy.context.view_layer.update()
    morphed = points()
    morph_delta = max((a-b).length for a, b in zip(rest, morphed))
    assert morph_delta > .09, (name, morph_delta)
    keys["Smile"].value = 0
    joint = rigs[0].pose.bones["jaw"]
    joint.rotation_mode = "QUATERNION"
    joint.rotation_quaternion = joint.rotation_quaternion @ Quaternion((0, 0, 1), .3)
    bpy.context.view_layer.update()
    bent = points()
    skin_delta = max((a-b).length for a, b in zip(rest, bent))
    assert skin_delta > .1, (name, skin_delta)
    results.append(dict(file=name, vertices=len(mesh.data.vertices), polygons=len(mesh.data.polygons),
                        triangles=4, bones=2, morph_displacement=morph_delta, skin_displacement=skin_delta))
(directory / "BlenderAuthoringVerification.json").write_text(
    json.dumps(dict(blender=bpy.app.version_string, results=results), indent=2) + "\n")
print(json.dumps(results))
