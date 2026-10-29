"""Read exported animation independently; no DCC application is started."""
import bisect
import json
import math
import struct
import subprocess
import sys


def fixture(mode):
    return subprocess.run([sys.argv[1], mode], check=True, text=True,
                          stdout=subprocess.PIPE).stdout.strip()


raw = bytes.fromhex(fixture("glb_fixture"))
magic, version, length = struct.unpack_from("<III", raw)
assert (magic, version, length) == (0x46546C67, 2, len(raw))
json_length, json_type = struct.unpack_from("<II", raw, 12)
assert json_type == 0x4E4F534A
doc = json.loads(raw[20:20 + json_length])
offset = 20 + json_length
binary_length, binary_type = struct.unpack_from("<II", raw, offset)
assert binary_type == 0x004E4942 and offset + 8 + binary_length == len(raw)
binary = raw[offset + 8:]


def accessor(index):
    item = doc["accessors"][index]
    assert item["componentType"] == 5126 and item["type"] == "SCALAR"
    view = doc["bufferViews"][item["bufferView"]]
    assert view["buffer"] == 0
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    return struct.unpack_from("<" + "f" * item["count"], binary, start)


def rotations(index):
    item = doc["accessors"][index]
    assert item["componentType"] == 5126 and item["type"] == "VEC4"
    view = doc["bufferViews"][item["bufferView"]]
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    values = struct.unpack_from("<" + "f" * (4 * item["count"]), binary, start)
    return list(zip(*(iter(values),) * 4))


def interpolate(times, values, t):
    i = bisect.bisect_right(times, t)
    if i == 0:
        return values[0]
    if i == len(times):
        return values[-1]
    factor = (t - times[i - 1]) / (times[i] - times[i - 1])
    return values[i - 1] + factor * (values[i] - values[i - 1])


seen = set()
elbow_rotation = False
for channel in doc["animations"][0]["channels"]:
    target = channel["target"]
    node = doc["nodes"][target["node"]]
    if target["path"] == "rotation" and node.get("name") == "elbow":
        sampler = doc["animations"][0]["samplers"][channel["sampler"]]
        times = accessor(sampler["input"])
        values = rotations(sampler["output"])
        assert times == (0.0, 1.0) and len(values) == 2
        assert math.isclose(values[0][3], 1, abs_tol=1e-6)
        assert math.isclose(abs(values[1][0]), math.sqrt(.5), abs_tol=1e-6)
        assert math.isclose(abs(values[1][3]), math.sqrt(.5), abs_tol=1e-6)
        elbow_rotation = True
    if target["path"] != "weights":
        continue
    mesh = doc["meshes"][node["mesh"]]
    names = mesh["extras"]["targetNames"]
    assert names == ["fold", "keep"]
    sampler = doc["animations"][0]["samplers"][channel["sampler"]]
    assert sampler["interpolation"] == "LINEAR"
    times, output = accessor(sampler["input"]), accessor(sampler["output"])
    assert times[0] == 0 and times[-1] == 1 and list(times) == sorted(set(times))
    assert len(output) == len(times) * len(names)
    for i, name in enumerate(names):
        values = output[i::len(names)]
        for sample in range(1001):
            t = sample / 1000
            expected = t * t * (3 - 2 * t) if name == "fold" else .2 + .6 * t
            assert math.isclose(interpolate(times, values, t), expected, abs_tol=.0003)
    seen.add(mesh["name"])
assert seen == {"body", "garment"}
assert elbow_rotation

subprocess.run([sys.argv[2], "corrective"], input=fixture("fbx_fixture"),
               text=True, check=True)

raw = bytes.fromhex(fixture("narrow_glb_fixture"))
magic, version, length = struct.unpack_from("<III", raw)
assert (magic, version, length) == (0x46546C67, 2, len(raw))
json_length, json_type = struct.unpack_from("<II", raw, 12)
assert json_type == 0x4E4F534A
doc = json.loads(raw[20:20 + json_length])
offset = 20 + json_length
binary_length, binary_type = struct.unpack_from("<II", raw, offset)
assert binary_type == 0x004E4942 and offset + 8 + binary_length == len(raw)
binary = raw[offset + 8:]
seen = set()
for channel in doc["animations"][0]["channels"]:
    target = channel["target"]
    if target["path"] != "weights":
        continue
    mesh = doc["meshes"][doc["nodes"][target["node"]]["mesh"]]
    names = mesh["extras"]["targetNames"]
    sampler = doc["animations"][0]["samplers"][channel["sampler"]]
    times, output = accessor(sampler["input"]), accessor(sampler["output"])
    assert names == ["fold", "keep"]
    values = output[0::len(names)]
    assert interpolate(times, values, .3) > .9989
    for sample in range(2800, 3201):
        t = sample / 10000
        influence = max(0, min(1, 1 - abs(t - .3) * math.pi / 2 / .02))
        expected = influence * influence * (3 - 2 * influence)
        assert math.isclose(interpolate(times, values, t), expected, abs_tol=.0011)
    seen.add(mesh["name"])
assert seen == {"body", "garment"}
subprocess.run([sys.argv[2], "corrective_narrow"], input=fixture("narrow_fbx_fixture"),
               text=True, check=True)
print("Independent GLB and FBX corrective animation readers passed")
