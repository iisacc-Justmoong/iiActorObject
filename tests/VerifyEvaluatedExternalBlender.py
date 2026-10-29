"""Edit a fitted human garment through the actual reusable Blender FBX adapter."""
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
session = module.ExternalSession.import_fbx(directory / "Tagged.fbx")
assert len(session.meshes) == 5 and len(session.rigs) == 1
assert len(session.rigs[0].data.bones) == 163
session.export_fbx(directory / "Baseline.fbx")
garment = next(obj for obj in session.meshes if int(obj[module.MESH]) == 4)
visible = {v for face in garment.data.polygons for v in face.vertices}
world = {i: garment.matrix_world @ garment.data.vertices[i].co for i in visible}
low = min(p.z for p in world.values())
high = max(p.z for p in world.values())
candidates = [i for i, p in world.items() if abs(p.x) < .12 and low + .25 * (high-low) < p.z < low + .65 * (high-low)]
assert candidates, (low, high)
point = min(candidates, key=lambda i: (world[i].y, i))
source_point = garment.data.attributes[module.POINT_ATTRIBUTE].data[point].value
delta = garment.matrix_world.to_3x3().inverted() @ Vector((0, -.003, 0))
if garment.data.shape_keys:
    for key in garment.data.shape_keys.key_blocks:
        key.data[point].co += delta
    garment.data.vertices[point].co = garment.data.shape_keys.key_blocks[0].data[point].co
else:
    garment.data.vertices[point].co += delta
bpy.context.view_layer.objects.active = garment
bpy.ops.object.select_all(action="DESELECT")
garment.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.sort_elements(type="REVERSE", elements={"VERT"})
bpy.ops.object.mode_set(mode="OBJECT")
new_index = next(i for i, p in enumerate(garment.data.attributes[module.POINT_ATTRIBUTE].data) if p.value == source_point)
assert new_index != point
session.export_fbx(directory / "Edited.fbx")
(directory / "ExpectedPoint.txt").write_text(str(source_point) + "\n")
bpy.ops.wm.save_as_mainfile(filepath=str(directory / "Edited.blend"))
report = {"blender": bpy.app.version_string, "meshes": 5, "bones": 163, "sourcePoint": source_point,
          "originalVertex": point, "editedVertex": new_index, "verticesReordered": True,
          "requestedBlenderWorldDeltaMetres": [0, -.003, 0], "expectedCoreDeltaMetres": [0, 0, .003],
          "worldBefore": list(world[point]), "visibleGarmentPoints": len(visible),
          "taggedInputSha256": hashlib.sha256((directory / "Tagged.fbx").read_bytes()).hexdigest()}
(directory / "BlenderEvaluatedExternalVerification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))
