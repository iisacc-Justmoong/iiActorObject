"""Check a generated GLB without linking the SDK or running a desktop app."""
import json
import math
import struct
import subprocess
import sys
from VerifyCharacterGlb import accessor as morph_accessor

raw = bytes.fromhex(subprocess.run([sys.argv[1], "fixture"], check=True, text=True,
                                   stdout=subprocess.PIPE).stdout.strip())
magic, version, length = struct.unpack_from("<III", raw)
assert (magic, version, length) == (0x46546C67, 2, len(raw))
json_length, json_type = struct.unpack_from("<II", raw, 12)
assert json_type == 0x4E4F534A
document = json.loads(raw[20:20 + json_length])
offset = 20 + json_length
binary_length, binary_type = struct.unpack_from("<II", raw, offset)
assert binary_type == 0x004E4942 and offset + 8 + binary_length == len(raw)
binary = raw[offset + 8:]


def accessor(index, non_vertex=True):
    item = document["accessors"][index]
    component = {5126: "f", 5123: "H", 5125: "I"}[item["componentType"]]
    count = {"SCALAR": 1, "VEC3": 3, "VEC4": 4, "MAT4": 16}[item["type"]]
    view = document["bufferViews"][item["bufferView"]]
    assert view["buffer"] == 0
    if non_vertex:
        assert "target" not in view
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    values = struct.unpack_from("<" + component * (item["count"] * count), binary, start)
    return item, [values[i:i + count] for i in range(0, len(values), count)]


assert len(document["animations"]) == 1
animation = document["animations"][0]
assert len(animation["channels"]) == len(animation["samplers"]) == 6
seen = {}
for channel in animation["channels"]:
    target = channel["target"]
    key = (target["node"], target["path"])
    assert key not in seen
    sampler = animation["samplers"][channel["sampler"]]
    assert sampler["interpolation"] == "LINEAR"
    time_accessor, times = accessor(sampler["input"])
    _, values = accessor(sampler["output"])
    times = [v[0] for v in times]
    assert len(times) == len(values) and times == sorted(set(times))
    assert time_accessor["min"] == [times[0]] and time_accessor["max"] == [times[-1]]
    seen[key] = (times, values)
assert set(seen) == {(node, path) for node in (0, 1) for path in ("translation", "rotation", "scale")}
for path in ("translation", "rotation", "scale"):
    times, values = seen[(1, path)]
    assert times == [0.0, .25, .75, 1.5, 2.0]
    assert values[0] == values[1] and values[-1] == values[-2]
assert seen[(0, "translation")][0] == [0.0, 2.0]
assert seen[(0, "translation")][1][-1] == (.5, 0.0, 0.0)
assert seen[(1, "translation")][1][2] == (0.0, 2.0, 0.0)
assert seen[(1, "scale")][1][2] == (1.0, 2.0, 1.0)
for q in seen[(1, "rotation")][1]:
    assert math.isclose(sum(v * v for v in q), 1.0, abs_tol=2e-7)
q = seen[(1, "rotation")][1][2]
assert math.isclose(q[2], math.sqrt(.5), abs_tol=1e-6)
assert math.isclose(q[3], math.sqrt(.5), abs_tol=1e-6)
q0, q1 = seen[(1, "rotation")][1][1:3]
dot = sum(a * b for a, b in zip(q0, q1))
theta = math.acos(min(1.0, abs(dot)))
coefficient = math.sin(theta * .5) / math.sin(theta)
mid = tuple(coefficient * (a + (1 if dot >= 0 else -1) * b) for a, b in zip(q0, q1))
assert math.isclose(mid[2], math.sin(math.pi / 8), abs_tol=1e-6)
assert math.isclose(mid[3], math.cos(math.pi / 8), abs_tol=1e-6)
skin = document["skins"][0]
assert skin["joints"] == [0, 1]
_, inverse = accessor(skin["inverseBindMatrices"])
attributes = document["meshes"][0]["primitives"][0]["attributes"]
_, positions = accessor(attributes["POSITION"], False)
_, joints = accessor(attributes["JOINTS_0"], False)
_, weights = accessor(attributes["WEIGHTS_0"], False)
assert joints[1][0] == 1 and weights[1][0] == 1.0
matrix, point = inverse[1], positions[1]
bind_x = matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12]
bind_y = matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13]
scale = seen[(1, "scale")][1][2]
translation = seen[(1, "translation")][1][2]
x, y = bind_x * scale[0], bind_y * scale[1]
z, w = q[2], q[3]
root_at_time = seen[(0, "translation")][1][-1][0] * (.75 / 2.0)
skin_x = (1 - 2*z*z)*x - 2*w*z*y + translation[0] + root_at_time
skin_y = 2*w*z*x + (1 - 2*z*z)*y + translation[1]
assert math.isclose(skin_x, 2.1875, abs_tol=1e-6)
assert math.isclose(skin_y, 3.0, abs_tol=1e-6)
raw = bytes.fromhex(subprocess.run([sys.argv[1], "retarget_fixture"], check=True, text=True,
                                   stdout=subprocess.PIPE).stdout.strip())
json_length, json_type = struct.unpack_from("<II", raw, 12)
assert json_type == 0x4E4F534A
document = json.loads(raw[20:20 + json_length])
offset = 20 + json_length
binary_length, binary_type = struct.unpack_from("<II", raw, offset)
assert binary_type == 0x004E4942 and offset + 8 + binary_length == len(raw)
binary = raw[offset + 8:]
animation = document["animations"][0]
retargeted = set()
for channel in animation["channels"]:
    retargeted.add((channel["target"]["node"], channel["target"]["path"]))
    sampler = animation["samplers"][channel["sampler"]]
    _, times = accessor(sampler["input"])
    _, values = accessor(sampler["output"])
    assert [t[0] for t in times] == [0.0, 1.0, 2.0]
    if channel["target"] == {"node": 0, "path": "translation"}:
        assert math.isclose(values[1][0], .5, abs_tol=1e-6)
    if channel["target"] == {"node": 1, "path": "rotation"}:
        assert math.isclose(values[1][2], math.sin(math.pi/8), abs_tol=1e-6)
        assert math.isclose(values[1][3], math.cos(math.pi/8), abs_tol=1e-6)
assert retargeted == {(node, path) for node in (0, 1) for path in ("translation", "rotation", "scale")}
raw = bytes.fromhex(subprocess.run([sys.argv[1], "dynamic_retarget_fixture"], check=True, text=True,
                                   stdout=subprocess.PIPE).stdout.strip())
json_length, json_type = struct.unpack_from("<II", raw, 12)
assert json_type == 0x4E4F534A
document = json.loads(raw[20:20 + json_length])
offset = 20 + json_length
binary_length, binary_type = struct.unpack_from("<II", raw, offset)
assert binary_type == 0x004E4942 and offset + 8 + binary_length == len(raw)
binary = raw[offset + 8:]
_, inverse = accessor(document["skins"][0]["inverseBindMatrices"])
assert math.isclose(inverse[1][13], -2.0, abs_tol=1e-6)
assert math.isclose(document["nodes"][1]["translation"][1], 2.0, abs_tol=1e-6)
seen = set()
for channel in document["animations"][0]["channels"]:
    target = channel["target"]
    seen.add((target["node"], target["path"]))
    sampler = document["animations"][0]["samplers"][channel["sampler"]]
    _, times = accessor(sampler["input"])
    _, values = accessor(sampler["output"])
    assert [t[0] for t in times] == [0.0, 1.0, 2.0]
    if target == {"node": 0, "path": "translation"}:
        assert math.isclose(values[1][0], 0.0, abs_tol=1e-6)
        assert math.isclose(values[1][1], .5, abs_tol=1e-6)
    if target == {"node": 1, "path": "rotation"}:
        assert math.isclose(abs(values[1][2]), math.sin(math.pi/8), abs_tol=1e-6)
        assert math.isclose(abs(values[1][3]), math.cos(math.pi/8), abs_tol=1e-6)
assert seen == {(node, path) for node in (0, 1) for path in ("translation", "rotation", "scale")}


def read_fixture(mode):
    raw = bytes.fromhex(subprocess.run([sys.argv[1], mode], check=True, text=True,
                                       stdout=subprocess.PIPE).stdout.strip())
    magic, version, length = struct.unpack_from("<III", raw)
    assert (magic, version, length) == (0x46546C67, 2, len(raw))
    json_length, json_type = struct.unpack_from("<II", raw, 12)
    assert json_type == 0x4E4F534A
    doc = json.loads(raw[20:20 + json_length])
    offset = 20 + json_length
    binary_length, binary_type = struct.unpack_from("<II", raw, offset)
    assert binary_type == 0x004E4942 and offset + 8 + binary_length == len(raw)
    return doc, raw[offset + 8:]


def sample(keys, t):
    if t <= keys[0][0]:
        return keys[0][1]
    if t >= keys[-1][0]:
        return keys[-1][1]
    for (a, x), (b, y) in zip(keys, keys[1:]):
        if a <= t <= b:
            return x + (y - x) * (t - a) / (b - a)
    raise AssertionError(t)


document, binary = read_fixture("morph_fixture")
animation = document["animations"][0]
assert len(animation["channels"]) == len(animation["samplers"]) == 5
seen = set()
curve_sources = {
    "Smile": [(.25, .1), (.75, .8), (1.75, .2)],
    "Blink": [(0, 0), (.5, 1), (2, 0)],
    "Tongue": [(.4, .25), (1.2, .9)],
    "Other": [(.4, .25), (1.2, .9)],
    "Unused": [(0, 0), (2, 0)],
}
for channel in animation["channels"]:
    target = channel["target"]
    identity = (target["node"], target["path"])
    assert identity not in seen
    seen.add(identity)
    if target["path"] != "weights":
        continue
    node = document["nodes"][target["node"]]
    mesh = document["meshes"][node["mesh"]]
    names = mesh["extras"]["targetNames"]
    assert names == {"Face": ["Smile", "Blink", "Unused"], "Mouth": ["Tongue", "Smile", "Other"]}[mesh["name"]]
    sampler = animation["samplers"][channel["sampler"]]
    assert sampler["interpolation"] == "LINEAR"
    time_info, time_values = accessor(sampler["input"])
    output_info, output_values = accessor(sampler["output"])
    times = [v[0] for v in time_values]
    assert time_info["min"] == [times[0]] and time_info["max"] == [times[-1]]
    assert times[0] == 0 and times[-1] == 2 and times == sorted(set(times))
    assert output_info["type"] == "SCALAR" and len(output_values) == len(times) * len(names)
    weights = [tuple(v[0] for v in output_values[i:i + len(names)]) for i in range(0, len(output_values), len(names))]
    for t in (0, .1, .25, .4, .5, .75, 1, 1.2, 1.75, 1.9, 2):
        for i, name in enumerate(names):
            actual = sample(list(zip(times, [w[i] for w in weights])), t)
            assert math.isclose(actual, sample(curve_sources[name], t), abs_tol=2e-6), (mesh["name"], name, t, actual)
    primitive = mesh["primitives"][0]
    positions = morph_accessor(document, binary, primitive["attributes"]["POSITION"])
    deltas = [morph_accessor(document, binary, item["POSITION"]) for item in primitive["targets"]]
    t = 1.0
    values = [sample(curve_sources[name], t) for name in names]
    point = [positions[0][axis] + sum(values[k] * deltas[k][0][axis] for k in range(len(names))) for axis in range(3)]
    if mesh["name"] == "Face":
        assert math.isclose(point[1], .2 * sample(curve_sources["Smile"], t), abs_tol=2e-6)
    else:
        assert math.isclose(point[2], .3 * sample(curve_sources["Tongue"], t), abs_tol=2e-6)
assert seen == {(1, "translation"), (1, "rotation"), (1, "scale"), (3, "weights"), (4, "weights")}

document, binary = read_fixture("static_morph_fixture")
assert "skins" not in document
animation = document["animations"][0]
assert {(channel["target"]["node"], channel["target"]["path"]) for channel in animation["channels"]} == {(1, "weights"), (2, "weights")}
for channel in animation["channels"]:
    sampler = animation["samplers"][channel["sampler"]]
    _, times = accessor(sampler["input"])
    _, values = accessor(sampler["output"])
    assert [t[0] for t in times] == [0, 1] and len(values) == 6

document, binary = read_fixture("scoped_morph_fixture")
indexed_document, indexed_binary = read_fixture("indexed_morph_fixture")
assert indexed_document == document and indexed_binary == binary
for channel in document["animations"][0]["channels"]:
    node = document["nodes"][channel["target"]["node"]]
    mesh = document["meshes"][node["mesh"]]
    names = mesh["extras"]["targetNames"]
    sampler = document["animations"][0]["samplers"][channel["sampler"]]
    _, values = accessor(sampler["output"])
    first = values[names.index("Smile")][0]
    last = values[len(names) + names.index("Smile")][0]
    assert (first, last) == {"Face": (0, 1), "Mouth": (1, 0)}[mesh["name"]]
document, binary = read_fixture("default_morph_fixture")
face = next(mesh for mesh in document["meshes"] if mesh["name"] == "Face")
assert face["weights"] == [0, 0, -.25]
animation = document["animations"][0]
for channel in animation["channels"]:
    node = document["nodes"][channel["target"]["node"]]
    mesh = document["meshes"][node["mesh"]]
    if mesh["name"] != "Face":
        continue
    _, weights = accessor(animation["samplers"][channel["sampler"]]["output"])
    assert [item[0] for item in weights] == [0, 0, -.25, 1, 0, -.25]
    break
else:
    raise AssertionError("Face morph animation was not exported")
print("independent GLB animation structure and samples passed")
