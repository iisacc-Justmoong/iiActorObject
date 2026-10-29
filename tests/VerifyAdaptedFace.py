"""Independently inspect adapted face geometry and skin in FBX and GLB."""
import json
import math
import pathlib
import struct
import sys
from collections import defaultdict
from FbxBinaryReader import child, read_fbx

fbx_path, glb_path = map(pathlib.Path, sys.argv[1:])
root = read_fbx(fbx_path.read_bytes())
objects = next(n for n in root if n[0] == "Objects")[2]
links = [n[1] for n in next(n for n in root if n[0] == "Connections")[2] if n[0] == "C" and n[1][0] == "OO"]
models = {n[1][0]: n[1][1].split("\0", 1)[0] for n in objects if n[0] == "Model" and n[1][2] == "LimbNode"}
meshes = {n[1][0]: n for n in objects if n[0] == "Geometry" and n[1][2] == "Mesh"}
skins = {n[1][0] for n in objects if n[0] == "Deformer" and n[1][2] == "Skin"}
clusters = {n[1][0]: n for n in objects if n[0] == "Deformer" and n[1][2] == "Cluster"}
pairs = {(n[1], n[2]) for n in links}
assert len(models) == 163 and {n[1][1].split("\0", 1)[0] for n in meshes.values()} == {"OBJ", "Eyes", "Teeth", "Tongue"}
fbx_bones = {}
for identity, mesh in meshes.items():
    name = mesh[1][1].split("\0", 1)[0]
    linked_skin = {s for s in skins if (s, identity) in pairs}
    assert len(linked_skin) == 1
    selected = {c for c in clusters if any((c, s) in pairs for s in linked_skin)}
    rows = defaultdict(dict)
    for cluster in selected:
        bone = [models[b] for b in models if (b, cluster) in pairs]
        assert len(bone) == 1
        indices = child(clusters[cluster], "Indexes")[1][0]
        values = child(clusters[cluster], "Weights")[1][0]
        assert len(indices) == len(values)
        for point, value in zip(indices, values):
            assert point >= 0 and math.isfinite(value) and 0 < value <= 1 and bone[0] not in rows[point]
            rows[point][bone[0]] = value
    visible = {v if v >= 0 else -v-1 for v in child(mesh, "PolygonVertexIndex")[1][0]}
    assert visible and all(rows[p] and abs(sum(rows[p].values()) - 1) < 1e-9 for p in visible)
    fbx_bones[name] = {bone for row in rows.values() for bone in row}

raw = glb_path.read_bytes()
assert struct.unpack_from("<III", raw) == (0x46546c67, 2, len(raw))
size, kind = struct.unpack_from("<II", raw, 12)
assert kind == 0x4e4f534a
doc = json.loads(raw[20:20 + size]); offset = 20 + size
bin_size, kind = struct.unpack_from("<II", raw, offset)
assert kind == 0x004e4942 and offset + 8 + bin_size == len(raw)
binary = raw[offset + 8:]
joint_names = [doc["nodes"][i]["name"] for i in doc["skins"][0]["joints"]]
assert len(joint_names) == 163
def accessor(index):
    item = doc["accessors"][index]; view = doc["bufferViews"][item["bufferView"]]
    assert item["type"] == "VEC4"
    code = {5121:"B", 5123:"H", 5125:"I", 5126:"f"}[item["componentType"]]
    width = struct.calcsize("<" + code); start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    stride = view.get("byteStride", 4 * width)
    return [struct.unpack_from("<" + code * 4, binary, start + i * stride) for i in range(item["count"])]
glb_bones = {}
for mesh in doc["meshes"]:
    used = set()
    for primitive in mesh["primitives"]:
        attrs = primitive["attributes"]; count = doc["accessors"][attrs["JOINTS_0"]]["count"]
        rows = [dict() for _ in range(count)]; slot = 0
        while f"JOINTS_{slot}" in attrs:
            joints, weights = accessor(attrs[f"JOINTS_{slot}"]), accessor(attrs[f"WEIGHTS_{slot}"])
            assert len(joints) == len(weights) == count
            for row, js, ws in zip(rows, joints, weights):
                for j, w in zip(js, ws):
                    assert math.isfinite(w) and 0 <= w <= 1
                    if w:
                        assert j < len(joint_names) and joint_names[j] not in row
                        row[joint_names[j]] = w
            slot += 1
        for row in rows:
            assert row and abs(sum(row.values()) - 1) < 2e-5
            used.update(row)
    glb_bones[mesh["name"]] = used

assert set(glb_bones) == set(fbx_bones) == {"OBJ", "Eyes", "Teeth", "Tongue"}
for groups in (fbx_bones, glb_bones):
    assert {"eye.L", "eye.R"} <= groups["Eyes"]
    assert "jaw" in groups["Teeth"]
    assert any(name.startswith("tongue") for name in groups["Tongue"])
print("Independent FBX/GLB: four adapted meshes, 163 joints and normalized face skin")
