"""Inspect generated FBX tangent layers without the SDK importer or desktop apps."""
import math
import os
import pathlib
import subprocess
import sys
import tempfile

from FbxBinaryReader import child, read_fbx


def produce(mode):
    return bytes.fromhex(subprocess.run([sys.argv[1], mode, sys.argv[2]], check=True,
                                        text=True, capture_output=True).stdout)


def mesh(mode):
    raw = produce(mode)
    root = read_fbx(raw)
    objects = next(x for x in root if x[0] == "Objects")[2]
    return next(x for x in objects if x[0] == "Geometry" and x[1][2] == "Mesh")


def by_type(node, kind):
    return {entry[1][0]: entry for entry in node[2] if entry[0] == kind}


def vectors(layer, field):
    values = child(layer, field)[1][0]
    assert len(values) % 3 == 0
    return [values[i:i+3] for i in range(0, len(values), 3)]


def close(a, b):
    return abs(a-b) < 2e-5


def verify(mode, expected_sets):
    node = mesh(mode)
    tangents = by_type(node, "LayerElementTangent")
    binormals = by_type(node, "LayerElementBinormal")
    layers = by_type(node, "Layer")
    assert set(tangents) == set(binormals) == set(expected_sets)
    normals_node = child(node, "LayerElementNormal")
    normal_values = vectors(normals_node, "Normals")
    normal_indices = child(normals_node, "NormalsIndex")[1][0]
    polygons = child(node, "PolygonVertexIndex")[1][0]
    normals = [normal_values[i] for i in normal_indices]
    assert len(polygons) == len(normals)
    for set_id in expected_sets:
        tangent_layer, binormal_layer = tangents[set_id], binormals[set_id]
        for layer in (tangent_layer, binormal_layer):
            assert child(layer, "MappingInformationType")[1][0] == "ByPolygonVertex"
            assert child(layer, "ReferenceInformationType")[1][0] == "Direct"
        t = vectors(tangent_layer, "Tangents")
        b = vectors(binormal_layer, "Binormals")
        signs = child(tangent_layer, "TangentsW")[1][0]
        assert len(t) == len(b) == len(signs) == len(polygons)
        declared = {(child(item, "Type")[1][0], child(item, "TypedIndex")[1][0])
                    for item in layers[set_id][2] if item[0] == "LayerElement"}
        assert {("LayerElementUV", set_id), ("LayerElementTangent", set_id),
                ("LayerElementBinormal", set_id)} <= declared
        for normal, tangent, binormal, sign in zip(normals, t, b, signs):
            assert sign in (-1, 1)
            assert all(math.isfinite(v) for v in (*normal, *tangent, *binormal))
            assert close(sum(v*v for v in tangent), 1) and close(sum(v*v for v in binormal), 1)
            assert close(sum(a*b for a, b in zip(normal, tangent)), 0)
            cross = (normal[1]*tangent[2]-normal[2]*tangent[1],
                     normal[2]*tangent[0]-normal[0]*tangent[2],
                     normal[0]*tangent[1]-normal[1]*tangent[0])
            assert all(close(a, sign*c) for a,c in zip(binormal,cross))
    return node, tangents


node, values = verify("triangle", {0})
signs = child(values[0], "TangentsW")[1][0]
assert signs == (1,1,1,-1,-1,-1)
assert [round(v[0]) for v in vectors(values[0], "Tangents")] == [1,1,1,-1,-1,-1]

node, values = verify("multi", {0, 1})
assert [round(v[1]) for v in vectors(values[1], "Tangents")] == [1]*6
assert child(values[1], "TangentsW")[1][0] == (-1,-1,-1,1,1,1)
for mode in ("quad", "reversed"):
    node, values = verify(mode, {0})
    assert len(child(node, "PolygonVertexIndex")[1][0]) == 4
    assert all(sign == (-1 if mode == "reversed" else 1) for sign in child(values[0], "TangentsW")[1][0])

node = mesh("conflict")
assert not by_type(node, "LayerElementTangent")
status = [n for n in node[2] if n[0] == "iiActorObjectTangentStatus"]
assert len(status) == 1 and child(status[0], "Reason")[1][0] == "polygon_tangent_conflict"

node = mesh("morph")
assert by_type(node, "LayerElementTangent")
status = [n for n in node[2] if n[0] == "iiActorObjectTangentStatus"]
assert len(status) == 1 and child(status[0], "Reason")[1][0] == "base_only_morphs"

node = mesh("no_normal")
assert not by_type(node, "LayerElementTangent")
assert not [n for n in node[2] if n[0] == "iiActorObjectTangentStatus"]
with tempfile.TemporaryDirectory(prefix="fbx-tangent-reader-", dir=os.getcwd()) as directory:
    file = pathlib.Path(directory) / "multi.fbx"
    file.write_bytes(produce("multi"))
    subprocess.run([sys.argv[3], str(file)], check=True)
print("Independent FBX tangent, binormal, UV, topology, morph and fallback checks passed")
