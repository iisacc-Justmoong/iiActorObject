"""Reload the merged FBX and independently check its coordinate, skin and face keys."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys
sys.dont_write_bytecode = True
import bpy
from mathutils import Vector

module_path, directory = map(Path, sys.argv[sys.argv.index("--") + 1:])
spec = importlib.util.spec_from_file_location("iiActorObjectBlenderExternal", module_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
bpy.ops.wm.read_factory_settings(use_empty=True)
source = directory / "Merged.fbx"
session = module.ExternalSession.import_fbx(source)
assert len(session.meshes) == 5 and len(session.rigs) == 1
rig = session.rigs[0]
assert len(rig.data.bones) == 163
garment = next(obj for obj in session.meshes if int(obj[module.MESH]) == 4)
body = next(obj for obj in session.meshes if int(obj[module.MESH]) == 0)
values = (directory / "MergedReference.txt").read_text().split()
source_point = int(values[0])
canonical = Vector(tuple(map(float, values[1:])))
expected = Vector((canonical.x, -canonical.z, canonical.y))
index = next(i for i, item in enumerate(garment.data.attributes[module.POINT_ATTRIBUTE].data) if item.value == source_point)

def evaluated_point(obj, vertex):
    bpy.context.view_layer.update()
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    result = evaluated.matrix_world @ mesh.vertices[vertex].co
    evaluated.to_mesh_clear()
    return result

morph_counts = {}
for obj in session.meshes:
    blocks = obj.data.shape_keys.key_blocks
    count = len(blocks) - 1
    mesh_id = int(obj[module.MESH])
    assert count == (60 if mesh_id == 4 else 59), (obj.name, count)
    morph_counts[str(mesh_id)] = count
    for key in blocks:
        key.value = 0
neutral = evaluated_point(garment, index)
error = (neutral - expected).length
assert error < 3e-6, (source_point, neutral, expected, error)
root = next(bone for bone in rig.data.bones if bone.parent is None)
pose = rig.pose.bones[root.name]
old_location = pose.location.copy()
world_frame = rig.matrix_world @ root.matrix_local
pose.location += world_frame.to_3x3().inverted() @ Vector((.005, 0, 0))
posed = evaluated_point(garment, index)
skin_movement = (posed - neutral).length
assert abs(skin_movement - .005) < 3e-6, (skin_movement, posed, neutral)
pose.location = old_location
blocks = body.data.shape_keys.key_blocks
jaw = blocks["pose/JawDrop"]
visible = {v for polygon in body.data.polygons for v in polygon.vertices}
jaw_point = max(visible, key=lambda i: (jaw.data[i].co - blocks[0].data[i].co).length_squared)
jaw_before = evaluated_point(body, jaw_point)
jaw.value = .5
jaw_after = evaluated_point(body, jaw_point)
jaw_movement = (jaw_after - jaw_before).length
assert jaw_movement > 1e-5, jaw_movement
jaw.value = 0
bpy.context.view_layer.update()
bpy.ops.wm.save_as_mainfile(filepath=str(directory / "MergedVerified.blend"))
report = {"blender": bpy.app.version_string, "meshes": 5, "bones": 163,
          "sourcePoint": source_point, "positionErrorMetres": error,
          "visibleSkinMovementMetres": skin_movement, "jawMorphMovementMetres": jaw_movement,
          "morphCounts": morph_counts, "posedClearanceVerified": False,
          "inputSha256": hashlib.sha256(source.read_bytes()).hexdigest()}
(directory / "BlenderMergedVerification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))
