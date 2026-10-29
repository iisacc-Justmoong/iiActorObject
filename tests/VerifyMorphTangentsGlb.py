"""Inspect generated tangent, skin and morph data without the SDK importer."""
import json
import math
import struct
import subprocess
import sys


raw = bytes.fromhex(subprocess.run([sys.argv[1], sys.argv[2]], check=True,
                                   text=True, capture_output=True).stdout)
assert struct.unpack_from("<III", raw) == (0x46546C67, 2, len(raw))
size, kind = struct.unpack_from("<II", raw, 12)
assert kind == 0x4E4F534A
doc = json.loads(raw[20:20 + size])
offset = 20 + size
binary_size, binary_kind = struct.unpack_from("<II", raw, offset)
assert binary_kind == 0x004E4942 and offset + 8 + binary_size == len(raw)
binary = raw[offset + 8:]


def accessor(index):
    item = doc["accessors"][index]
    count = item["count"]
    components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[item["type"]]
    formats = {5123: "H", 5125: "I", 5126: "f"}
    code = formats[item["componentType"]]
    width = struct.calcsize("<" + code)
    values = [[0] * components for _ in range(count)]
    if "bufferView" in item:
        view = doc["bufferViews"][item["bufferView"]]
        start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
        stride = view.get("byteStride", components * width)
        for i in range(count):
            values[i] = list(struct.unpack_from("<" + code * components, binary, start + stride * i))
    if "sparse" in item:
        sparse = item["sparse"]
        indices = sparse["indices"]
        iv = doc["bufferViews"][indices["bufferView"]]
        ic = formats[indices["componentType"]]
        istart = iv.get("byteOffset", 0) + indices.get("byteOffset", 0)
        vv = doc["bufferViews"][sparse["values"]["bufferView"]]
        vstart = vv.get("byteOffset", 0) + sparse["values"].get("byteOffset", 0)
        for i in range(sparse["count"]):
            target = struct.unpack_from("<" + ic, binary, istart + i * struct.calcsize("<" + ic))[0]
            values[target] = list(struct.unpack_from("<" + code * components, binary, vstart + i * width * components))
    return values


def close(a, b):
    return abs(a - b) < 2e-5


primitive = doc["meshes"][0]["primitives"][0]
attributes = primitive["attributes"]
assert "TANGENT" in attributes
positions = accessor(attributes["POSITION"])
normals = accessor(attributes["NORMAL"])
tangents = accessor(attributes["TANGENT"])
uvs = accessor(attributes["TEXCOORD_0"])
joints = accessor(attributes["JOINTS_0"])
weights = accessor(attributes["WEIGHTS_0"])
indices = [x[0] for x in accessor(primitive["indices"])]
target = primitive["targets"][0]
assert "TANGENT" in target
position_delta = accessor(target["POSITION"])
normal_delta = accessor(target["NORMAL"])
tangent_delta = accessor(target["TANGENT"])
assert len(indices) == 6
for corner, index in enumerate(indices):
    t = tangents[index]
    assert t[3] == (-1 if corner < 3 else 1)
    assert close(t[0], 1 if corner < 3 else -1) and close(t[1], 0) and close(t[2], 0)
    target_t = [t[i] + tangent_delta[index][i] for i in range(3)]
    expected = [(.8 if corner < 3 else -.8), 0, (-.6 if corner < 3 else .6)]
    assert all(close(a, b) for a, b in zip(target_t, expected))
    target_n = [normals[index][i] + normal_delta[index][i] for i in range(3)]
    assert close(sum(a*b for a, b in zip(target_n, target_t)), 0)
    assert close(sum(x*x for x in target_t), 1)
    p = positions[index]
    delta = position_delta[index]
    assert close(delta[0], -.2*p[0]) and close(delta[2], -.6*p[0])
    assert all(math.isfinite(v) for v in uvs[index])
    assert close(sum(weights[index]), 1)
    if close(p[0], 1):
        assert joints[index][:2] == [0, 1] and close(weights[index][0], .75) and close(weights[index][1], .25)
assert indices[0] != indices[3], "Mirrored tangent seam needs distinct exported vertices"
print("Independent GLB tangent, morph, mirrored seam and skin checks passed")


def load_variant(mode):
    raw = bytes.fromhex(subprocess.run([sys.argv[1], sys.argv[2], mode], check=True,
                                       text=True, capture_output=True).stdout)
    assert struct.unpack_from("<III", raw) == (0x46546C67, 2, len(raw))
    size, kind = struct.unpack_from("<II", raw, 12)
    assert kind == 0x4E4F534A
    parsed = json.loads(raw[20:20 + size])
    start = 20 + size
    length, kind = struct.unpack_from("<II", raw, start)
    assert kind == 0x004E4942 and start + 8 + length == len(raw)
    return parsed, raw[start + 8:]


doc, binary = load_variant("multiple_uv")
primitives = doc["meshes"][0]["primitives"]
assert len(primitives) == 2
for primitive in primitives:
    material = doc["materials"][primitive["material"]]
    selected = material["normalTexture"]["texCoord"]
    tangents = accessor(primitive["attributes"]["TANGENT"])
    indices = [entry[0] for entry in accessor(primitive["indices"])]
    assert len(indices) == 3
    expected = [1, 0, 0] if selected == 0 else [0, 1, 0]
    assert selected in (0, 1)
    for i in indices:
        assert all(close(a, b) for a, b in zip(tangents[i][:3], expected))
        assert tangents[i][3] == -1
assert {doc["materials"][p["material"]]["normalTexture"]["texCoord"] for p in primitives} == {0, 1}

for mode in ("invalid_normal", "plain"):
    doc, binary = load_variant(mode)
    primitive = doc["meshes"][0]["primitives"][0]
    assert "TANGENT" not in primitive["attributes"]
    assert "POSITION" in primitive["targets"][0] and "NORMAL" in primitive["targets"][0]
    if mode == "invalid_normal":
        assert primitive["extras"]["iiActorObjectTangentFallback"] == "invalid_basis"
    else:
        assert "extras" not in primitive
print("Independent GLB material UV selection and tangent fallback checks passed")

doc, binary = load_variant("fbx_roundtrip")
primitive = doc["meshes"][0]["primitives"][0]
assert "source_fbx_morph_tangents_unavailable" in primitive["extras"]["iiActorObjectFbxTangentNotes"]
attributes = primitive["attributes"]
positions = accessor(attributes["POSITION"])
tangents = accessor(attributes["TANGENT"])
joints = accessor(attributes["JOINTS_0"])
weights = accessor(attributes["WEIGHTS_0"])
indices = [entry[0] for entry in accessor(primitive["indices"])]
target = primitive["targets"][0]
assert "POSITION" in target and "NORMAL" in target and "TANGENT" not in target
position_delta = accessor(target["POSITION"])
assert len(indices) == 6
weighted = 0
for corner, index in enumerate(indices):
    tangent = tangents[index]
    assert tangent[3] == (-1 if corner < 3 else 1)
    assert close(tangent[0], 1 if corner < 3 else -1) and close(tangent[1], 0) and close(tangent[2], 0)
    assert close(sum(weights[index]), 1)
    if close(positions[index][0], 1):
        assert joints[index][:2] == [0, 1] and close(weights[index][0], .75) and close(weights[index][1], .25)
        weighted += 1
    assert close(position_delta[index][0], -.2 * positions[index][0])
    assert close(position_delta[index][2], -.6 * positions[index][0])
assert weighted >= 1
print("Independent FBX to GLB tangent seam, morph, rig and skin checks passed")

doc, binary = load_variant("fbx_roundtrip_mixed")
primitives = doc["meshes"][0]["primitives"]
assert len(primitives) == 2 and len(doc["skins"][0]["joints"]) == 2
for primitive in primitives:
    notes = primitive["extras"]["iiActorObjectFbxTangentNotes"]
    fallback = "source_fbx_handedness_unknown" in notes
    assert "source_fbx_morph_tangents_unavailable" in notes
    attrs = primitive["attributes"]
    positions = accessor(attrs["POSITION"])
    tangents = accessor(attrs["TANGENT"])
    weights = accessor(attrs["WEIGHTS_0"])
    indices = [entry[0] for entry in accessor(primitive["indices"])]
    assert len(indices) == 3
    target = primitive["targets"][0]
    assert "POSITION" in target and "NORMAL" in target
    assert ("TANGENT" in target) == fallback
    position_delta = accessor(target["POSITION"])
    for index in indices:
        expected = (-1, 0, 0, 1) if fallback else (1, 0, 0, -1)
        assert all(close(a, b) for a, b in zip(tangents[index], expected))
        assert close(sum(weights[index]), 1)
        assert close(position_delta[index][0], -.2 * positions[index][0])
        assert close(position_delta[index][2], -.6 * positions[index][0])
print("Independent mixed-source FBX to GLB preserved face-specific skin and morphs")
