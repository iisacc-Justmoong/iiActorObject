"""Actual application hierarchy, inverse-bind and posed-skin verification after core rig edits."""
from pathlib import Path
import json
import math
import struct
import sys
import bpy
from mathutils import Matrix, Vector, Quaternion
from mathutils.kdtree import KDTree

unit_dir, human_dir = map(Path, sys.argv[sys.argv.index("--") + 1:])
results = []
def reference(path):
    raw = path.read_bytes()
    size = struct.unpack_from("<I", raw, 12)[0]
    doc, binary = json.loads(raw[20:20 + size]), raw[28 + size:]
    def values(index, components):
        accessor = doc["accessors"][index]
        view = doc["bufferViews"][accessor["bufferView"]]
        code, width = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}[accessor["componentType"]]
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        return [struct.unpack_from("<" + code * components, binary, start + i * view.get("byteStride", components * width))
                for i in range(accessor["count"])]
    nodes, worlds = doc["nodes"], {}
    parents = {child: i for i, node in enumerate(nodes) for child in node.get("children", [])}
    def world(i):
        if i not in worlds:
            node = nodes[i]
            q = node.get("rotation", [0, 0, 0, 1])
            local = Matrix.LocRotScale(Vector(node.get("translation", [0, 0, 0])),
                                      Quaternion((q[3], q[0], q[1], q[2])), Vector(node.get("scale", [1, 1, 1])))
            worlds[i] = world(parents[i]) @ local if i in parents else local
        return worlds[i]
    conversion = Matrix.Rotation(math.pi / 2, 4, "X")
    sources = {}
    for i, mesh in enumerate(doc["meshes"]):
        matrix = conversion @ world(next(n for n, node in enumerate(nodes) if node.get("mesh") == i))
        points = []
        for primitive in mesh["primitives"]:
            positions = values(primitive["attributes"]["POSITION"], 3)
            visible = set(v[0] for v in values(primitive["indices"], 1))
            points.extend(matrix @ Vector(positions[v]) for v in visible)
        sources[mesh["name"]] = points
    skin = doc["skins"][0]
    error = 0
    for joint, entries in zip(skin["joints"], values(skin["inverseBindMatrices"], 16)):
        inverse = Matrix([entries[i:i + 4] for i in range(0, 16, 4)]).transposed()
        product = world(joint) @ inverse
        error = max(error, max(abs(product[r][c] - (r == c)) for r in range(4) for c in range(4)))
    assert error < 2e-6, error
    heads = {nodes[i]["name"]: conversion @ world(i).translation for i in skin["joints"]}
    return doc, sources, heads, error
def point_set_error(a, b):
    def directed(source, target):
        tree = KDTree(len(target))
        for i, point in enumerate(target):
            tree.insert(point, i)
        tree.balance()
        return max(tree.find(point)[2] for point in source)
    return max(directed(a, b), directed(b, a))

for directory, stem, human in ((unit_dir, "EditedSkeleton", False), (human_dir, "EditedHuman", True)):
    doc, sources, heads, source_bind_error = reference(directory / f"{stem}.glb")
    for suffix in ("glb", "fbx"):
        path = directory / f"{stem}.{suffix}"
        bpy.ops.wm.read_factory_settings(use_empty=True)
        if suffix == "glb":
            raw = path.read_bytes()
            size = struct.unpack_from("<I", raw, 12)[0]
            doc = json.loads(raw[20:20 + size])
            if not human:
                assert doc["nodes"][1]["name"] == "elbow" and 1 in doc["nodes"][3]["children"]
            bpy.ops.import_scene.gltf(filepath=str(path))
        else:
            bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=True)
        rigs = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
        meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"
                  and any(mod.type == "ARMATURE" for mod in obj.modifiers)]
        assert len(rigs) == 1 and len(meshes) == (2 if human else 1), (path.name, [(o.name, o.type, [m.type for m in o.modifiers]) for o in bpy.context.scene.objects])
        rig = rigs[0]
        assert len(rig.data.bones) == (163 if human else 4)
        child, parent = ("spine01", "spine03") if human else ("elbow", "mount")
        assert rig.data.bones[child].parent.name == parent
        if not human:
            assert rig.data.bones["hand"].parent.name == "elbow"
        def positions(obj):
            evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
            mesh = evaluated.to_mesh()
            result = [evaluated.matrix_world @ v.co for v in mesh.vertices]
            evaluated.to_mesh_clear()
            return result
        rest = {obj.name: positions(obj) for obj in meshes}
        visible = {obj.name: {v for face in obj.data.polygons for v in face.vertices} for obj in meshes}
        # The importer retargets mesh.data to its own unit-scale edit-bone rest.
        # Compare evaluated geometry to the original file, not that altered base mesh.
        internal_base_difference = max((rest[obj.name][i] - obj.matrix_world @ obj.data.vertices[i].co).length
                                       for obj in meshes for i in visible[obj.name])
        source_error = max(point_set_error([rest[obj.name][i] for i in visible[obj.name]],
                                          sources[next(name for name in sources if name in (obj.name, obj.data.name))])
                           for obj in meshes)
        head_error = max((rig.matrix_world @ rig.pose.bones[name].matrix.translation - point).length
                         for name, point in heads.items())
        tolerance = 1e-4 if human else 2e-6
        assert source_error < tolerance and head_error < tolerance, (path.name, source_error, head_error)
        def rotate(name):
            bone = rig.pose.bones[name]
            pivot = rig.matrix_world @ bone.matrix.translation
            delta = Matrix.Translation(pivot) @ Matrix.Rotation(.2, 4, "Y") @ Matrix.Translation(-pivot)
            bone.matrix = rig.matrix_world.inverted() @ delta @ rig.matrix_world @ bone.matrix
            bpy.context.view_layer.update()
            return delta
        delta = rotate(parent)
        posed = {obj.name: positions(obj) for obj in meshes}
        movement = max((posed[obj.name][i] - rest[obj.name][i]).length
                       for obj in meshes for i in visible[obj.name])
        assert movement > .001 and all(math.isfinite(x) for pts in posed.values() for v in pts for x in v)
        record = {"file": path.name, "bones": len(rig.data.bones), "meshes": len(meshes),
                  "parent": parent, "child": child, "sourceGeometryErrorMetres": source_error,
                  "sourceJointPositionErrorMetres": head_error, "sourcePositionToleranceMetres": tolerance,
                  "sourceBindMatrixError": source_bind_error, "importerBaseDifferenceMetres": internal_base_difference,
                  "skinMovementMetres": movement}
        if not human:
            full_error = max((posed[obj.name][i] - delta @ rest[obj.name][i]).length
                             for obj in meshes for i in visible[obj.name])
            assert full_error < 2e-6, full_error
            for bone in rig.pose.bones:
                bone.matrix_basis = Matrix.Identity(4)
            bpy.context.view_layer.update()
            delta = rotate("hand")
            partial_error = max((positions(obj)[i] - (.8 * (delta @ rest[obj.name][i]) + .2 * rest[obj.name][i])).length
                                for obj in meshes for i in visible[obj.name])
            assert partial_error < 2e-6, partial_error
            record.update(fullInfluenceErrorMetres=full_error, partialInfluenceErrorMetres=partial_error)
        results.append(record)
report = {"blender": bpy.app.version_string, "results": results}
(unit_dir / "BlenderSkeletonEditingVerification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))
