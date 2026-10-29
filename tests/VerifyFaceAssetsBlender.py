"""Check original geometry/heads, packed face textures and independent eye/jaw skin."""
from pathlib import Path
import hashlib
import json
import sys
import bpy
from mathutils import Vector, Quaternion

directory, assets = map(Path, sys.argv[sys.argv.index("--") + 1:])
reference = json.loads((directory / "FaceHumanReference.json").read_text())
images = {"EyesMaterial": assets / "eyes/materials/brown_eye.png",
          "TeethMaterial": assets / "teeth/teeth_base/teeth.png",
          "TongueMaterial": assets / "tongue/tongue01/tongue01_diffuse.png"}
convert = lambda p: Vector((p[0], -p[2], p[1]))
results = []
def reaches_base_color(tree, source):
    pending = [source]
    visited = set()
    while pending:
        node = pending.pop()
        if node.as_pointer() in visited:
            continue
        visited.add(node.as_pointer())
        for link in tree.links:
            if link.from_node != node:
                continue
            if link.to_node.type == "BSDF_PRINCIPLED" and link.to_socket.name == "Base Color":
                return True
            pending.append(link.to_node)
    return False
for extension, key in (("glb", "renderPoints"), ("fbx", "sourcePoints")):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    filename = str(directory / ("FaceHuman." + extension))
    status = bpy.ops.import_scene.gltf(filepath=filename) if extension == "glb" else bpy.ops.import_scene.fbx(filepath=filename)
    assert status == {"FINISHED"}
    rigs = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH" and any(mod.type == "ARMATURE" for mod in obj.modifiers)]
    assert len(rigs) == 1 and len(meshes) == 4 and len(rigs[0].data.bones) == 163
    rig = rigs[0]
    def positions(obj):
        evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        mesh = evaluated.to_mesh()
        points = [evaluated.matrix_world @ vertex.co for vertex in mesh.vertices]
        evaluated.to_mesh_clear()
        return points
    position_errors = {}
    for expected in reference["meshes"]:
        obj = next(obj for obj in meshes if obj.name == expected["name"])
        actual = positions(obj)
        assert len(actual) == len(expected[key])
        error = max((a - convert(b)).length for a, b in zip(actual, expected[key]))
        assert error < 2e-6, (extension, obj.name, error)
        position_errors[obj.name] = error
    joint_error = max((rig.matrix_world @ rig.data.bones[name].head_local - convert(point)).length for name, point in reference["joints"].items())
    assert joint_error < 2e-6
    texture_hashes = {}
    for name, source in images.items():
        material = bpy.data.materials[name]
        nodes = [node for node in material.node_tree.nodes if node.type == "TEX_IMAGE" and node.image]
        assert nodes, (extension, name)
        for node in nodes:
            imported = node.image
            assert imported.size[0] > 0 and imported.size[1] > 0
            encoded = bytes(imported.packed_file.data) if imported.packed_file else Path(bpy.path.abspath(imported.filepath)).read_bytes()
            assert hashlib.sha256(encoded).hexdigest() == hashlib.sha256(source.read_bytes()).hexdigest(), (extension, name)
        color_nodes = [node for node in nodes if reaches_base_color(material.node_tree, node)]
        assert len(color_nodes) == 1, (extension, name)
        if color_nodes[0].image.depth == 32:
            assert any(link.to_socket.name == "Alpha" and link.from_socket.name == "Alpha" for link in material.node_tree.links), (extension, name, "PNG alpha is not connected")
        image = color_nodes[0].image
        encoded = bytes(image.packed_file.data) if image.packed_file else Path(bpy.path.abspath(image.filepath)).read_bytes()
        digest = hashlib.sha256(encoded).hexdigest()
        assert digest == hashlib.sha256(source.read_bytes()).hexdigest(), (extension, name)
        texture_hashes[name] = digest
    eye = next(obj for obj in meshes if obj.name == "Eyes")
    original = positions(eye)
    bone = rig.pose.bones["eye.L"]
    bone.rotation_mode = "QUATERNION"
    previous = bone.rotation_quaternion.copy()
    bone.rotation_quaternion = previous @ Quaternion((0, 0, 1), .3)
    bpy.context.view_layer.update()
    moved = positions(eye)
    left_motion = max((a-b).length for a, b in zip(moved, original) if b.x > 0)
    right_motion = max((a-b).length for a, b in zip(moved, original) if b.x < 0)
    assert left_motion > .001 and right_motion < 2e-6, (extension, left_motion, right_motion)
    bone.rotation_quaternion = previous
    teeth = next(obj for obj in meshes if obj.name == "Teeth")
    bpy.context.view_layer.update()
    teeth_rest = positions(teeth)
    bone = rig.pose.bones["jaw"]
    bone.rotation_mode = "QUATERNION"
    previous = bone.rotation_quaternion.copy()
    bone.rotation_quaternion = previous @ Quaternion((1, 0, 0), .3)
    bpy.context.view_layer.update()
    jaw_motion = max((a-b).length for a, b in zip(positions(teeth), teeth_rest))
    assert jaw_motion > .001
    bone.rotation_quaternion = previous
    bpy.context.view_layer.update()
    bpy.ops.wm.save_as_mainfile(filepath=str(directory / ("FaceHuman-" + extension + ".blend")))
    results.append({"format": extension, "meshes": 4, "bones": 163, "positionErrorsMetres": position_errors,
                    "jointErrorMetres": joint_error, "textureSha256": texture_hashes,
                    "eyeMotionMetres": left_motion, "oppositeEyeMotionMetres": right_motion, "jawMotionMetres": jaw_motion})
(directory / "BlenderFaceAssetsVerification.json").write_text(json.dumps({"blender": bpy.app.version_string, "results": results}, indent=2) + "\n")
print(json.dumps(results))
