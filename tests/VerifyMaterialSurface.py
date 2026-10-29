"""Independently inspect material surface fields in generated GLB and FBX."""
import json
import math
import struct
import subprocess
import sys


def output(mode):
    result = subprocess.run([sys.argv[1], mode], check=True, text=True, capture_output=True)
    return bytes.fromhex(result.stdout.strip())


glb = output("surface_glb")
magic, version, size = struct.unpack_from("<III", glb)
assert (magic, version, size) == (0x46546C67, 2, len(glb))
json_size, json_type = struct.unpack_from("<II", glb, 12)
assert json_type == 0x4E4F534A
document = json.loads(glb[20:20 + json_size])
materials = {material["name"]: material for material in document["materials"]}
assert {"matA", "matB"} <= materials.keys()
first, second = materials["matA"], materials["matB"]
assert first["alphaMode"] == "MASK" and first["alphaCutoff"] == .25 and first["doubleSided"] is True
assert second["alphaMode"] == "OPAQUE" and second["alphaCutoff"] == 1.25
assert not second.get("doubleSided", False)
assert math.isclose(first["pbrMetallicRoughness"]["baseColorFactor"][3], .4)
assert math.isclose(second["pbrMetallicRoughness"]["baseColorFactor"][3], .2)
assert all("baseColorTexture" in item["pbrMetallicRoughness"] for item in (first, second))

raw = output("surface_fbx")
assert raw[:23] == b"Kaydara FBX Binary  \x00\x1a\x00" and struct.unpack_from("<I", raw, 23)[0] == 7400


def properties(start, count, length):
    end, result = start + length, []
    for _ in range(count):
        code = chr(raw[start])
        start += 1
        if code in "YCILFD":
            form = {"Y": "h", "C": "?", "I": "i", "L": "q", "F": "f", "D": "d"}[code]
            result.append(struct.unpack_from("<" + form, raw, start)[0])
            start += struct.calcsize(form)
        elif code in "SR":
            length = struct.unpack_from("<I", raw, start)[0]
            start += 4
            value = raw[start:start + length]
            result.append(value.decode("utf-8") if code == "S" else value)
            start += length
        elif code in "fdilbc":
            _, _, length = struct.unpack_from("<III", raw, start)
            start += 12 + length
            result.append(None)
        else:
            raise AssertionError(f"Unexpected FBX property type: {code}")
    assert start == end
    return result


def nodes(start, limit):
    result = []
    while start + 13 <= limit:
        end, count, size, name_size = struct.unpack_from("<IIIB", raw, start)
        if not end:
            break
        assert start < end <= limit
        name = raw[start + 13:start + 13 + name_size].decode("ascii")
        begin = start + 13 + name_size
        assert begin + size <= end
        result.append((name, properties(begin, count, size), nodes(begin + size, end)))
        start = end
    return result


objects = next(node[2] for node in nodes(27, len(raw)) if node[0] == "Objects")
fbx_materials = {node[1][1].split("\x00", 1)[0]: node for node in objects if node[0] == "Material"}
for name, mode, cutoff, sides, original_alpha, opacity in (
    ("matA", 2, .25, 1, .4, .4), ("matB", 1, 1.25, 0, .2, 1.0)
):
    props = next(child[2] for child in fbx_materials[name][2] if child[0] == "Properties70")
    values = {entry[1][0]: entry[1][-1] for entry in props if entry[0] == "P"}
    assert values["iiAlphaMode"] == mode and values["iiAlphaCutoff"] == cutoff
    assert values["iiDoubleSided"] == sides
    assert math.isclose(values["iiBaseColorAlpha"], original_alpha)
    assert math.isclose(values["Opacity"], opacity)
print("Independent GLB surface fields and FBX metadata/opacity passed")
