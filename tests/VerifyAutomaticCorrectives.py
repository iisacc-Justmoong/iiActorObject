"""Read GLB independently and check generated morphs against rigid DQ arithmetic."""
import json
import math
import struct
import subprocess
import sys

raw = bytes.fromhex(subprocess.run([sys.argv[1], "encoded"], check=True, capture_output=True, text=True).stdout.strip())
magic, version, length = struct.unpack_from("<4sII", raw)
assert magic == b"glTF" and version == 2 and length == len(raw)
json_size, json_type = struct.unpack_from("<I4s", raw, 12)
assert json_type == b"JSON"
doc = json.loads(raw[20:20 + json_size].decode("utf-8"))
offset = 20 + json_size
bin_size, bin_type = struct.unpack_from("<I4s", raw, offset)
assert bin_type == b"BIN\0" and offset + 8 + bin_size == len(raw)
binary = raw[offset + 8:]

formats = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
dimensions = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}


def accessor(index):
    desc = doc["accessors"][index]
    assert not desc.get("normalized")
    fmt, size = formats[desc["componentType"]]
    count = dimensions[desc["type"]]
    if "bufferView" in desc:
        view = doc["bufferViews"][desc["bufferView"]]
        stride = view.get("byteStride", count * size)
        start = view.get("byteOffset", 0) + desc.get("byteOffset", 0)
        values = [struct.unpack_from("<" + fmt * count, binary, start + stride * i) for i in range(desc["count"])]
    else:
        values = [(0,) * count for _ in range(desc["count"])]
    if "sparse" in desc:
        sparse = desc["sparse"]
        indices = sparse["indices"]
        index_fmt, index_size = formats[indices["componentType"]]
        index_view = doc["bufferViews"][indices["bufferView"]]
        index_start = index_view.get("byteOffset", 0) + indices.get("byteOffset", 0)
        value_view = doc["bufferViews"][sparse["values"]["bufferView"]]
        value_start = value_view.get("byteOffset", 0) + sparse["values"].get("byteOffset", 0)
        previous = -1
        for i in range(sparse["count"]):
            target = struct.unpack_from("<" + index_fmt, binary, index_start + index_size * i)[0]
            assert previous < target < len(values)
            previous = target
            values[target] = struct.unpack_from("<" + fmt * count, binary, value_start + count * size * i)
    return values


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def scale(a, value):
    return tuple(value * x for x in a)


def rotate(a, angle):
    c, s = math.cos(angle), math.sin(angle)
    return (c * a[0] - s * a[1], s * a[0] + c * a[1], a[2])


def normalized(v):
    return scale(v, 1 / math.sqrt(sum(x * x for x in v)))


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


checked = 0
nonzero = 0
for node in doc["nodes"]:
    if "mesh" not in node:
        continue
    mesh = doc["meshes"][node["mesh"]]
    skin = doc["skins"][node["skin"]]
    joint_names = [doc["nodes"][j]["name"] for j in skin["joints"]]
    names = mesh["extras"]["targetNames"]
    assert names == ["existing", "auto_elbow"]
    for primitive in mesh["primitives"]:
        attrs = primitive["attributes"]
        positions, normals = accessor(attrs["POSITION"]), accessor(attrs["NORMAL"])
        indices, weights = accessor(attrs["JOINTS_0"]), accessor(attrs["WEIGHTS_0"])
        existing = primitive["targets"][0]
        corrective = primitive["targets"][1]
        ep, en = accessor(existing["POSITION"]), accessor(existing["NORMAL"])
        cp, cn = accessor(corrective["POSITION"]), accessor(corrective["NORMAL"])
        for p, n, js, ws, dp, dn, cpos, cnorm in zip(positions, normals, indices, weights, ep, en, cp, cn):
            assert abs(sum(ws) - 1) < 1e-6
            original_p, original_n = add(p, scale(dp, .35)), add(n, scale(dn, .35))
            corrected_p, corrected_n = add(original_p, scale(cpos, .7)), add(original_n, scale(cnorm, .7))
            actual_p, actual_n = (0., 0., 0.), (0., 0., 0.)
            for joint, weight in zip(js, ws):
                if not weight:
                    continue
                name = joint_names[joint]
                assert name in ("root", "elbow")
                angle = 0. if name == "root" else math.pi / 2
                actual_p = add(actual_p, scale(rotate(corrected_p, angle), weight))
                actual_n = add(actual_n, scale(rotate(corrected_n, angle), weight))
            assert dist(actual_p, rotate(original_p, math.pi / 4)) < 3e-6
            assert dist(normalized(actual_n), normalized(rotate(original_n, math.pi / 4))) < 3e-6
            nonzero += dist(cpos, (0, 0, 0)) > 1e-7
            checked += 1
assert checked >= 7 and nonzero > 0
print(f"Independent GLB joint, morph and DQ comparison passed for {checked} vertices")
