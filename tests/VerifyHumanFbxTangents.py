"""Independently inspect a generated human FBX tangent basis and rig."""
import math
import pathlib
import sys

from FbxBinaryReader import child, read_fbx

root = read_fbx(pathlib.Path(sys.argv[1]).read_bytes())
objects = next(node for node in root if node[0] == "Objects")[2]
geometry = next(node for node in objects if node[0] == "Geometry" and node[1][2] == "Mesh")
models = [node for node in objects if node[0] == "Model" and node[1][2] == "LimbNode"]
assert len(models) == 163
corners = child(geometry, "PolygonVertexIndex")[1][0]
assert len(corners) == 53512 and sum(value < 0 for value in corners) == 13378


def vec3(values):
    assert len(values) % 3 == 0
    return [values[i:i+3] for i in range(0, len(values), 3)]


normal_layer = child(geometry, "LayerElementNormal")
normal_values = vec3(child(normal_layer, "Normals")[1][0])
normal_indices = child(normal_layer, "NormalsIndex")[1][0]
tangent_layer = child(geometry, "LayerElementTangent")
binormal_layer = child(geometry, "LayerElementBinormal")
tangents = vec3(child(tangent_layer, "Tangents")[1][0])
binormals = vec3(child(binormal_layer, "Binormals")[1][0])
signs = child(tangent_layer, "TangentsW")[1][0]
assert len(normal_indices) == len(tangents) == len(binormals) == len(signs) == len(corners)
for index, t, b, sign in zip(normal_indices, tangents, binormals, signs):
    n = normal_values[index]
    length = math.sqrt(sum(value*value for value in n))
    assert length > 0
    n = [value/length for value in n]
    assert sign in (-1, 1)
    assert abs(sum(value*value for value in t)-1) < 3e-4
    assert abs(sum(value*value for value in b)-1) < 3e-4
    assert abs(sum(a*c for a,c in zip(n,t))) < 3e-4
    cross = (n[1]*t[2]-n[2]*t[1], n[2]*t[0]-n[0]*t[2], n[0]*t[1]-n[1]*t[0])
    assert all(abs(a-sign*c) < 3e-4 for a,c in zip(b,cross))
status = [node for node in geometry[2] if node[0] == "iiActorObjectTangentStatus"]
assert len(status) == 1 and child(status[0], "Reason")[1][0] == "base_only_morphs"
assert any(node[0] == "Deformer" and node[1][2] == "Skin" for node in objects)
assert any(node[0] == "Deformer" and node[1][2] == "BlendShapeChannel" for node in objects)
print(f"Independent human FBX: {len(corners)} polygon corners, {len(models)} joints, base-only morph status")
