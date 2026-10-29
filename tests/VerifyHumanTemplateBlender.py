"""실제 Blender의 CC0 템플릿 가져오기·바인드·팔 변형 검사이다. 런타임 의존성이 아니다."""
from pathlib import Path
import json
import math
import sys
import bpy
from mathutils import Quaternion

directory = Path(sys.argv[sys.argv.index("--") + 1])
results = []
for name in ("Human.glb", "Human.fbx", "HumanEdited.glb"):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source = directory / name
    if source.suffix == ".glb":
        bpy.ops.import_scene.gltf(filepath=str(source))
    else:
        bpy.ops.import_scene.fbx(filepath=str(source), use_custom_normals=True)
    rigs = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    inventory = [(o.name, o.type, [m.type for m in o.modifiers]) for o in bpy.context.scene.objects]
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"
              and any(m.type == "ARMATURE" for m in o.modifiers)]
    assert len(rigs) == 1 and len(rigs[0].data.bones) == 163, name
    assert len(meshes) == 1, (name, inventory)
    obj, rig = meshes[0], rigs[0]
    assert len(obj.data.vertices) > 13000, name
    obj.data.calc_loop_triangles()
    assert len(obj.data.loop_triangles) == 26756, (name, len(obj.data.loop_triangles))
    if source.suffix == ".fbx":
        assert len(obj.data.polygons) == 13378 and len(obj.data.vertices) == 19158, name
    graph = bpy.context.evaluated_depsgraph_get()
    def positions():
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        points = [evaluated.matrix_world @ vertex.co for vertex in mesh.vertices]
        evaluated.to_mesh_clear()
        return points
    original = [obj.matrix_world @ v.co for v in obj.data.vertices]
    rest = positions()
    rest_error = max((a - b).length for a, b in zip(original, rest))
    assert rest_error < 2e-5, (name, rest_error)
    bone = rig.pose.bones["lowerarm01.L"]
    bone.rotation_mode = "QUATERNION"
    bone.rotation_quaternion = bone.rotation_quaternion @ Quaternion((0, 0, 1), .5)
    bpy.context.view_layer.update()
    posed = positions()
    displacement = max((a - b).length for a, b in zip(rest, posed))
    assert displacement > .03, (name, displacement)
    assert all(math.isfinite(c) for point in posed for c in point)
    results.append(dict(file=name, vertices=len(rest), bones=163,
                        polygons=len(obj.data.polygons),triangles=len(obj.data.loop_triangles), bind_error=rest_error,
                        pose_displacement=displacement,
                        unskinned_meshes=[o.name for o in bpy.context.scene.objects
                                         if o.type == "MESH" and o not in meshes]))
(directory / "BlenderTemplateVerification.json").write_text(
    json.dumps(dict(blender=bpy.app.version_string, results=results), indent=2) + "\n")
print(json.dumps(results))
