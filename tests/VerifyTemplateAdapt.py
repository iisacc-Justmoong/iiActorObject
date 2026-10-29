"""Independently check adapted runtime morphs in FBX and GLB without desktop apps."""
import json
import math
import pathlib
import struct
import sys
from FbxBinaryReader import child, read_fbx

fbx_path, glb_path = map(pathlib.Path, sys.argv[1:])
root = read_fbx(fbx_path.read_bytes())
objects = next(node for node in root if node[0] == "Objects")[2]
links = [node[1] for node in next(node for node in root if node[0] == "Connections")[2] if node[0] == "C"]
channels = {node[1][1].split("\x00", 1)[0]: node for node in objects
            if node[0] == "Deformer" and node[1][2] == "BlendShapeChannel"}
shapes = {node[1][0]: node for node in objects if node[0] == "Geometry" and node[1][2] == "Shape"}
assert set(channels) == {"rise"}
channel = channels["rise"]
attached = [link[1] for link in links if len(link) == 3 and link[0] == "OO" and link[2] == channel[1][0] and link[1] in shapes]
assert len(attached) == 1
shape = shapes[attached[0]]
indices, values = child(shape, "Indexes")[1][0], child(shape, "Vertices")[1][0]
assert len(indices) >= 5 and len(values) == len(indices) * 3
for i in range(len(indices)):
    assert math.isclose(values[i*3], 0, abs_tol=1e-9)
    assert math.isclose(values[i*3+1], 0, abs_tol=1e-9)
    assert math.isclose(values[i*3+2], .02, abs_tol=1e-8)

raw = glb_path.read_bytes()
assert struct.unpack_from("<III", raw) == (0x46546c67, 2, len(raw))
size, kind = struct.unpack_from("<II", raw, 12)
assert kind == 0x4e4f534a
doc = json.loads(raw[20:20+size]); offset = 20+size
bin_size, kind = struct.unpack_from("<II", raw, offset)
assert kind == 0x004e4942 and offset+8+bin_size == len(raw)
binary = raw[offset+8:]
mesh = doc["meshes"][0]
assert mesh["extras"]["targetNames"] == ["rise"]
accessor = doc["accessors"][mesh["primitives"][0]["targets"][0]["POSITION"]]
assert accessor["componentType"] == 5126 and accessor["type"] == "VEC3" and accessor["count"] == 5
sparse = accessor["sparse"]
assert sparse["count"] == 5 and sparse["indices"]["componentType"] == 5125
def offset_of(entry):
    view = doc["bufferViews"][entry["bufferView"]]
    return view.get("byteOffset", 0) + entry.get("byteOffset", 0)
ids = struct.unpack_from("<5I", binary, offset_of(sparse["indices"]))
deltas = struct.unpack_from("<15f", binary, offset_of(sparse["values"]))
assert ids == (0, 1, 2, 3, 4)
for i in range(5):
    assert all(math.isclose(a, b, abs_tol=1e-7) for a, b in zip(deltas[i*3:i*3+3], (0, 0, .02)))
print("Independent FBX/GLB: adapted rise morph and all visible position deltas")
