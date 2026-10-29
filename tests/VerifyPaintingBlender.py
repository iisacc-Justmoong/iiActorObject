"""Compare actual Blender rest/posed meshes and locked groups before/after painting."""
from pathlib import Path
import json
import sys
import bpy
from mathutils import Quaternion

directory = Path(sys.argv[sys.argv.index("--") + 1])
def load(name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = directory / name
    if path.suffix == ".glb":
        bpy.ops.import_scene.gltf(filepath=str(path))
    else:
        bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=True)
    rigs = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    assert len(rigs) == 1 and len(rigs[0].data.bones) == 163
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"
              and any(m.type == "ARMATURE" for m in o.modifiers)]
    assert len(meshes) == 2
    graph = bpy.context.evaluated_depsgraph_get()
    def points(obj):
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        result = [evaluated.matrix_world @ v.co for v in mesh.vertices]
        evaluated.to_mesh_clear()
        return result
    body = next(o for o in meshes if "GarmentProxy" not in o.name)
    visible = {i for p in body.data.polygons for i in p.vertices}
    rest = points(body)
    def group(name):
        entry = body.vertex_groups.get(name)
        if entry is None:
            return [0.] * len(body.data.vertices)
        return [next((g.weight for g in v.groups if g.group == entry.index), 0.) for v in body.data.vertices]
    weights, locked = group("spine02"), group("spine03")
    bone = rigs[0].pose.bones["spine02"]
    bone.rotation_mode = "QUATERNION"
    bone.rotation_quaternion = bone.rotation_quaternion @ Quaternion((0, 0, 1), .4)
    bpy.context.view_layer.update()
    return {"rest": rest, "posed": points(body), "visible": visible, "weights": weights, "locked": locked}

results = []
for extension in ("glb", "fbx"):
    before = load("Before." + extension)
    after = load("Painted." + extension)
    assert len(before["rest"]) == len(after["rest"]) and before["visible"] == after["visible"]
    ids = before["visible"]
    rest_error = max((before["rest"][i] - after["rest"][i]).length for i in ids)
    pose_change = max((before["posed"][i] - after["posed"][i]).length for i in ids)
    weight_change = max(abs(before["weights"][i] - after["weights"][i]) for i in ids)
    locked_change = max(abs(before["locked"][i] - after["locked"][i]) for i in ids)
    assert rest_error < 2e-6 and pose_change > .001 and weight_change > .05 and locked_change < 2e-6
    results.append({"format": extension, "visibleVertices": len(ids), "restDifferenceMetres": rest_error,
                    "posedDifferenceMetres": pose_change, "maximumWeightChange": weight_change,
                    "lockedWeightDifference": locked_change})
report = {"blender": bpy.app.version_string, "results": results}
(directory / "BlenderPaintingVerification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))
