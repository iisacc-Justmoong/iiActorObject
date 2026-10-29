"""Read both exported formats without using the SDK or launching a DCC app."""
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
    assert item["componentType"] == 5126
    width = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[item["type"]]
    view = doc["bufferViews"][item["bufferView"]]
    assert view["buffer"] == 0
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    values = struct.unpack_from("<" + "f" * (item["count"] * width), binary, start)
    return [values[i:i + width] for i in range(0, len(values), width)]


def sample(times, values, time):
    i = bisect.bisect_right(times, time)
    if i == 0:
        return values[0]
    if i == len(times):
        return values[-1]
    factor = (time - times[i - 1]) / (times[i] - times[i - 1])
    return tuple(a + factor * (b - a) for a, b in zip(values[i - 1], values[i]))


seen = set()
animation = doc["animations"][0]
for channel in animation["channels"]:
    target = channel["target"]
    node = doc["nodes"][target["node"]]
    if target["path"] == "weights":
        mesh = doc["meshes"][node["mesh"]]
        assert mesh["extras"]["targetNames"] == ["Smile"]
        kind = mesh["name"]
        assert kind in {"body", "garment"}
    elif target["path"] == "translation" and node.get("name") == "joint":
        kind = "joint"
    else:
        continue
    sampler = animation["samplers"][channel["sampler"]]
    assert sampler["interpolation"] == "LINEAR"
    times = [value[0] for value in accessor(sampler["input"])]
    values = accessor(sampler["output"])
    assert times[0] == 0 and times[-1] == 1 and times == sorted(set(times))
    for i in range(101):
        t = i / 100
        value = sample(times, values, t)[0]
        expected = t if kind == "body" else 1 - t if kind == "garment" else 2 * t
        assert math.isclose(value, expected, abs_tol=2e-5), (kind, t, value)
    seen.add(kind)
assert seen == {"body", "garment", "joint"}
subprocess.run([sys.argv[2], "baked_glb"], input=fixture("fbx_fixture"), text=True, check=True)
print("Independent GLB and FBX readers verified baked joint and per-mesh morph curves")
