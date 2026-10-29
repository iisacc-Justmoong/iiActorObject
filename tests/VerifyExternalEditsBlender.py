"""Edit a real source-tagged asset, reorder its vertices and export through the reusable adapter."""
from pathlib import Path
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
assert len(session.rigs) == 1 and len(session.rigs[0].data.bones) == 163
assert len(session.meshes) == 2
assert all(abs(material.roughness - float(material["iiRoughness"])) < 1e-6
           for obj in session.meshes for material in obj.data.materials)
session.export_fbx(directory / "Baseline.fbx")
body = next(obj for obj in session.meshes if int(obj[module.MESH]) == 0)
visible = {v for face in body.data.polygons for v in face.vertices}
point = min(visible, key=lambda v: (body.matrix_world @ body.data.vertices[v].co - Vector((0, 0, 1.2))).length)
source_point = body.data.attributes[module.POINT_ATTRIBUTE].data[point].value
delta = body.matrix_world.to_3x3().inverted() @ Vector((.003, 0, 0))
if body.data.shape_keys:
    for key in body.data.shape_keys.key_blocks:
        key.data[point].co += delta
    body.data.vertices[point].co = body.data.shape_keys.key_blocks[0].data[point].co
else:
    body.data.vertices[point].co += delta
for group in body.vertex_groups:
    group.remove([point])
for name, value in (("spine01", .75), ("spine03", .25)):
    group = body.vertex_groups.get(name) or body.vertex_groups.new(name=name)
    group.add([point], value, "REPLACE")
if body.data.uv_layers:
    for loop in body.data.uv_layers.active.data:
        loop.uv.x += .002
material = body.data.materials[0]
shader = next(node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED")
shader.inputs["Roughness"].default_value = .42
material.roughness = .42
reattached = module.ExternalSession(document=session.document)
assert abs(material.roughness - .42) < 1e-6
rig = session.rigs[0]
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="EDIT")
parent = rig.data.edit_bones["spine03"]
for bone in parent.children:
    bone.use_connect = False
parent.head.x += .01
parent.tail.x += .01
child = rig.data.edit_bones["spine01"]
child.use_connect = False
child.parent = rig.data.edit_bones["spine02"]
bpy.ops.object.mode_set(mode="OBJECT")
rig.data.bones["spine01"].name = "external_spine01"
body.name = "ExternalBody"
bpy.context.view_layer.objects.active = body
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.sort_elements(type="REVERSE", elements={"VERT"})
bpy.ops.object.mode_set(mode="OBJECT")
current = next(i for i, item in enumerate(body.data.attributes[module.POINT_ATTRIBUTE].data) if item.value == source_point)
assert current != point, (point, current)
session.export_fbx(directory / "Edited.fbx")
(directory / "ExpectedPoint.txt").write_text(str(source_point) + "\n")
bpy.ops.wm.save_as_mainfile(filepath=str(directory / "Edited.blend"))
report = {"blender": bpy.app.version_string, "sourcePoint": source_point, "originalVertex": point,
          "editedVertex": current, "verticesReordered": True, "bones": 163, "meshes": 2,
          "meshDeltaMetres": .003, "jointDeltaMetres": .01, "weights": [.75, .25],
          "uvShift": .002, "roughness": .42, "jointReparented": True,
          "jointRenamed": True, "meshRenamed": True}
(directory / "BlenderExternalEditsVerification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))

bpy.ops.wm.read_factory_settings(use_empty=True)
morph_session = module.ExternalSession.import_fbx(directory / "MorphTagged.fbx")
morph_session.export_fbx(directory / "MorphBaseline.fbx")
obj = morph_session.meshes[0]
key = obj.data.shape_keys.key_blocks["Smile"]
key.name = "Grin"
key.data[2].co += obj.matrix_world.to_3x3().inverted() @ Vector((.02, 0, 0))
blink = obj.shape_key_add(name="Blink", from_mix=False)
blink.data[0].co += obj.matrix_world.to_3x3().inverted() @ Vector((.01, 0, 0))
bpy.context.view_layer.objects.active = obj
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.sort_elements(type="REVERSE", elements={"VERT"})
bpy.ops.object.mode_set(mode="OBJECT")
assert obj.data.attributes[module.POINT_ATTRIBUTE].data[0].value == 4
morph_session.export_fbx(directory / "MorphEdited.fbx")
bpy.ops.wm.save_as_mainfile(filepath=str(directory / "MorphEdited.blend"))
report["morphFixture"] = {"sourcePoints": 5, "sourceTarget": 42, "renamed": "Grin", "added": "Blink",
                           "verticesReordered": True, "existingDeltaXMetres": .02, "addedDeltaXMetres": .01}
(directory / "BlenderExternalEditsVerification.json").write_text(json.dumps(report, indent=2) + "\n")
