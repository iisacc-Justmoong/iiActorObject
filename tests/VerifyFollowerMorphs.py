"""Independent FBX/GLB graph and position-delta check, without desktop apps."""
import json
import math
import pathlib
import struct
import sys
from FbxBinaryReader import child, read_fbx

fbx_path, glb_path = map(pathlib.Path, sys.argv[1:3])
channel_name, expected = sys.argv[3], set(sys.argv[4].split(","))
expected_z = float(sys.argv[5]) if len(sys.argv) > 5 else None
root = read_fbx(fbx_path.read_bytes())
objects = next(n for n in root if n[0] == "Objects")[2]
links = {(n[1][1], n[1][2]) for n in next(n for n in root if n[0] == "Connections")[2]
         if n[0] == "C" and n[1][0] == "OO"}
meshes = {n[1][0]: n for n in objects if n[0] == "Geometry" and n[1][2] == "Mesh"}
shapes = {n[1][0]: n for n in objects if n[0] == "Geometry" and n[1][2] == "Shape"}
blends = {n[1][0] for n in objects if n[0] == "Deformer" and n[1][2] == "BlendShape"}
channels = {n[1][0]: n for n in objects if n[0] == "Deformer" and n[1][2] == "BlendShapeChannel"
            and n[1][1].split("\0", 1)[0] == channel_name}
fbx_found = {}
for mesh_id, mesh in meshes.items():
    name = mesh[1][1].split("\0", 1)[0]
    related = [c for c in channels if any((c, b) in links and (b, mesh_id) in links for b in blends)]
    if not related:
        continue
    assert len(related) == 1
    selected = [shape for identity, shape in shapes.items() if (identity, related[0]) in links]
    assert len(selected) == 1
    indices, values = child(selected[0], "Indexes")[1][0], child(selected[0], "Vertices")[1][0]
    assert indices and len(values) == 3 * len(indices) and all(math.isfinite(v) for v in values)
    assert any(abs(v) > 1e-9 for v in values)
    if expected_z is not None:
        assert all(math.isclose(values[3*i+2], expected_z, abs_tol=1e-8) for i in range(len(indices)))
    fbx_found[name] = len(indices)
assert set(fbx_found) == expected, fbx_found

raw = glb_path.read_bytes()
assert struct.unpack_from("<III", raw) == (0x46546c67, 2, len(raw))
size, kind = struct.unpack_from("<II", raw, 12)
assert kind == 0x4e4f534a
doc = json.loads(raw[20:20+size]); offset = 20+size
bin_size, kind = struct.unpack_from("<II", raw, offset)
assert kind == 0x004e4942 and offset+8+bin_size == len(raw)
binary = raw[offset+8:]
def offset_of(entry):
    return doc["bufferViews"][entry["bufferView"]].get("byteOffset", 0) + entry.get("byteOffset", 0)

glb_found = {}
for mesh in doc["meshes"]:
    names = mesh.get("extras", {}).get("targetNames", [])
    if channel_name not in names:
        continue
    index = names.index(channel_name); magnitude = 0; seen = 0
    for primitive in mesh["primitives"]:
        accessor = doc["accessors"][primitive["targets"][index]["POSITION"]]
        assert accessor["componentType"] == 5126 and accessor["type"] == "VEC3"
        if "sparse" in accessor:
            sparse = accessor["sparse"]; count = sparse["count"]
            values = struct.unpack_from("<"+"f"*(3*count), binary, offset_of(sparse["values"]))
        else:
            count = accessor["count"]
            values = struct.unpack_from("<"+"f"*(3*count), binary, offset_of(accessor))
        assert all(math.isfinite(v) for v in values)
        if expected_z is not None:
            assert all(math.isclose(values[3*i+2], expected_z, abs_tol=1e-7) for i in range(count))
        magnitude = max(magnitude, *(abs(v) for v in values)); seen += count
    assert magnitude > 1e-9
    glb_found[mesh["name"]] = seen
assert set(glb_found) == expected, glb_found
print(f"Independent FBX/GLB: {channel_name} on {sorted(expected)}; FBX points {fbx_found}; GLB points {glb_found}")
