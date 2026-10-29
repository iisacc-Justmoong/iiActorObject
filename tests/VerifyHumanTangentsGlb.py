"""Independently validate tangents, morph deltas and skin in the human GLB fixture."""
import json
import math
import pathlib
import struct
import sys

raw = pathlib.Path(sys.argv[1]).read_bytes()
assert struct.unpack_from("<III", raw) == (0x46546C67, 2, len(raw))
json_size, kind = struct.unpack_from("<II", raw, 12)
assert kind == 0x4E4F534A
doc = json.loads(raw[20:20 + json_size])
bin_offset = 20 + json_size
bin_size, kind = struct.unpack_from("<II", raw, bin_offset)
assert kind == 0x004E4942 and bin_offset + 8 + bin_size == len(raw)
binary = raw[bin_offset + 8:]


def accessor(index):
    item = doc["accessors"][index]
    count = item["count"]
    components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[item["type"]]
    code = {5123: "H", 5125: "I", 5126: "f"}[item["componentType"]]
    width = struct.calcsize("<" + code)
    values = [[0] * components for _ in range(count)]
    if "bufferView" in item:
        view = doc["bufferViews"][item["bufferView"]]
        start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
        stride = view.get("byteStride", components * width)
        for i in range(count):
            values[i] = struct.unpack_from("<" + code * components, binary, start + stride * i)
    if "sparse" in item:
        sparse = item["sparse"]
        index = sparse["indices"]
        view = doc["bufferViews"][index["bufferView"]]
        index_start = view.get("byteOffset", 0) + index.get("byteOffset", 0)
        index_code = {5121: "B", 5123: "H", 5125: "I"}[index["componentType"]]
        value = sparse["values"]
        value_view = doc["bufferViews"][value["bufferView"]]
        value_start = value_view.get("byteOffset", 0) + value.get("byteOffset", 0)
        for i in range(sparse["count"]):
            destination = struct.unpack_from("<" + index_code, binary,
                                             index_start + i * struct.calcsize("<" + index_code))[0]
            assert destination < count
            values[destination] = struct.unpack_from("<" + code * components, binary,
                                                     value_start + i * components * width)
    return values


assert len(doc["skins"]) == 1 and len(doc["skins"][0]["joints"]) == 163
primitives = doc["meshes"][0]["primitives"]
assert len(primitives) == 1
primitive = primitives[0]
attrs = primitive["attributes"]
assert "iiActorObjectTangentFallback" not in primitive.get("extras", {})
assert doc["materials"][primitive["material"]]["normalTexture"]["texCoord"] == 0
positions = accessor(attrs["POSITION"])
normals = accessor(attrs["NORMAL"])
tangents = accessor(attrs["TANGENT"])
uv = accessor(attrs["TEXCOORD_0"])
indices = accessor(primitive["indices"])
assert len(positions) == len(normals) == len(tangents) == len(uv) == len(indices) == 80268
assert all(entry[0] < len(positions) for entry in indices)
target = primitive["targets"][0]
assert {"POSITION", "NORMAL", "TANGENT"} <= set(target)
normal_delta = accessor(target["NORMAL"])
tangent_delta = accessor(target["TANGENT"])
assert len(normal_delta) == len(tangent_delta) == len(positions)
changed = 0
for n, t, nd, td in zip(normals, tangents, normal_delta, tangent_delta):
    assert all(math.isfinite(x) for x in (*n, *t, *nd, *td))
    assert t[3] in (-1, 1)
    assert abs(sum(x*x for x in t[:3]) - 1) < 3e-4
    assert abs(sum(a*b for a, b in zip(n, t))) < 3e-4
    if any(abs(x) > 1e-8 for x in td):
        changed += 1
        mn = [n[i] + nd[i] for i in range(3)]
        mt = [t[i] + td[i] for i in range(3)]
        assert abs(sum(x*x for x in mt) - 1) < 5e-4
        assert abs(sum(a*b for a, b in zip(mn, mt))) < 5e-4
assert changed > 0
weight_sets = [accessor(attrs[key]) for key in sorted(attrs) if key.startswith("WEIGHTS_")]
joint_sets = [accessor(attrs[key]) for key in sorted(attrs) if key.startswith("JOINTS_")]
assert weight_sets and len(weight_sets) == len(joint_sets)
for vertex in range(len(positions)):
    assert abs(sum(sum(weights[vertex]) for weights in weight_sets) - 1) < 2e-5
    for joints, weights in zip(joint_sets, weight_sets):
        assert all(joint < 163 for joint, weight in zip(joints[vertex], weights[vertex]) if weight != 0)
print(f"Independent human GLB: {len(positions)} corners, {changed} morph tangent deltas, 163 joints")
