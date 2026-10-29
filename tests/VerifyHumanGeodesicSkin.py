"""Independently inspect skin weights in a generated FBX and GLB human."""
import json
import math
import pathlib
import struct
import sys
from collections import defaultdict
from FbxBinaryReader import child, read_fbx

fbx, glb = map(pathlib.Path, sys.argv[1:])
root = read_fbx(fbx.read_bytes())
objects = next(n for n in root if n[0] == "Objects")[2]
mesh = next(n for n in objects if n[0] == "Geometry" and n[1][2] == "Mesh")
assert sum(n[0] == "Model" and n[1][2] == "LimbNode" for n in objects) == 163
polygons = child(mesh, "PolygonVertexIndex")[1][0]
visible = {v if v >= 0 else -v-1 for v in polygons}
rows = defaultdict(list)
clusters = [n for n in objects if n[0] == "Deformer" and n[1][2] == "Cluster"]
assert len(clusters) > 10
for cluster in clusters:
    indices = child(cluster, "Indexes")[1][0]
    weights = child(cluster, "Weights")[1][0]
    assert len(indices) == len(weights)
    for index, value in zip(indices, weights):
        assert index >= 0 and math.isfinite(value) and 0 < value <= 1
        rows[index].append(value)
for point in visible:
    assert 1 <= len(rows[point]) <= 4
    assert abs(sum(rows[point])-1) < 1e-9

raw = glb.read_bytes()
assert struct.unpack_from("<III", raw) == (0x46546c67,2,len(raw))
size, kind = struct.unpack_from("<II",raw,12)
assert kind == 0x4e4f534a
doc = json.loads(raw[20:20+size]);offset = 20+size
bin_size, kind = struct.unpack_from("<II",raw,offset)
assert kind == 0x004e4942 and offset+8+bin_size == len(raw)
binary = raw[offset+8:]
assert len(doc["skins"]) == 1 and len(doc["skins"][0]["joints"]) == 163

def accessor(index):
    item = doc["accessors"][index];view = doc["bufferViews"][item["bufferView"]]
    count = item["count"];components = {"SCALAR":1,"VEC3":3,"VEC4":4}[item["type"]]
    code = {5121:"B",5123:"H",5125:"I",5126:"f"}[item["componentType"]]
    width = struct.calcsize("<"+code);start = view.get("byteOffset",0)+item.get("byteOffset",0)
    stride = view.get("byteStride",components*width)
    return [struct.unpack_from("<"+code*components,binary,start+i*stride) for i in range(count)]

vertices = 0
for primitive in doc["meshes"][0]["primitives"]:
    attributes = primitive["attributes"]
    assert "JOINTS_0" in attributes and "WEIGHTS_0" in attributes
    assert "JOINTS_1" not in attributes and "WEIGHTS_1" not in attributes
    joints, weights = accessor(attributes["JOINTS_0"]),accessor(attributes["WEIGHTS_0"])
    assert len(joints) == len(weights)
    vertices += len(weights)
    for ji, wi in zip(joints, weights):
        assert all(math.isfinite(v) and 0 <= v <= 1 for v in wi)
        assert abs(sum(wi)-1) < 2e-5
        assert all(j < 163 for j,w in zip(ji,wi) if w > 0)
assert vertices > 10_000
print(f"Independent FBX/GLB skin: {len(visible)} source points, {vertices} render vertices, 163 joints")
