"""Independently inspect mixed-attribute source and exported skinned GLB geometry."""
import json
import math
import struct
import subprocess
import sys

from VerifyCharacterGlb import accessor


def fixture(mode):
    result = subprocess.run([sys.argv[1], mode], check=True, text=True, capture_output=True)
    raw = bytes.fromhex(result.stdout.strip())
    magic, version, length = struct.unpack_from("<III", raw)
    assert (magic, version, length) == (0x46546C67, 2, len(raw))
    json_length, json_tag = struct.unpack_from("<II", raw, 12)
    assert json_tag == 0x4E4F534A
    document = json.loads(raw[20:20 + json_length])
    offset = 20 + json_length
    bin_length, bin_tag = struct.unpack_from("<II", raw, offset)
    assert bin_tag == 0x004E4942 and offset + 8 + bin_length == len(raw)
    return document, raw[offset + 8:]


source, _ = fixture("mixed_source")
primitives = source["meshes"][0]["primitives"]
assert len(primitives) == 2
assert "NORMAL" in primitives[0]["attributes"] and "TEXCOORD_1" in primitives[0]["attributes"]
assert "NORMAL" not in primitives[1]["attributes"] and "TEXCOORD_0" not in primitives[1]["attributes"]
assert primitives[0]["attributes"]["POSITION"] == primitives[1]["attributes"]["POSITION"]

document, binary = fixture("mixed_export")
mesh = document["meshes"][0]
assert mesh["extras"]["targetNames"] == ["Raise"]
assert len(mesh["primitives"]) == 2 and len(document["skins"]) == 1
checked = set()
for primitive in mesh["primitives"]:
    name = document["materials"][primitive["material"]]["name"]
    assert name in {"A", "B"} and name not in checked
    checked.add(name)
    attributes = primitive["attributes"]
    positions = accessor(document, binary, attributes["POSITION"])
    normals = accessor(document, binary, attributes["NORMAL"])
    uv0 = accessor(document, binary, attributes["TEXCOORD_0"])
    uv1 = accessor(document, binary, attributes["TEXCOORD_1"])
    weights = accessor(document, binary, attributes["WEIGHTS_0"])
    indices = [value[0] for value in accessor(document, binary, primitive["indices"])]
    assert len(indices) == 3
    target = primitive["targets"][0]
    position_deltas = accessor(document, binary, target["POSITION"])
    normal_deltas = accessor(document, binary, target["NORMAL"])
    for vertex in indices:
        assert abs(sum(weights[vertex]) - 1) < 1e-6
        actual_position = [positions[vertex][i] + position_deltas[vertex][i] for i in range(3)]
        actual_normal = [normals[vertex][i] + normal_deltas[vertex][i] for i in range(3)]
        magnitude = math.sqrt(sum(value * value for value in actual_normal))
        actual_normal = [value / magnitude for value in actual_normal]
        if name == "A":
            expected_normal = [.2 / math.sqrt(1.04), 1 / math.sqrt(1.04), 0]
            assert abs(actual_position[2]) < 1e-6
        else:
            expected_normal = [-.5 / math.sqrt(1.5), -.5 / math.sqrt(1.5), 1 / math.sqrt(1.5)]
            assert abs(uv0[vertex][0]) < 1e-6 and abs(uv0[vertex][1] - 1) < 1e-6
            assert abs(uv1[vertex][0]) < 1e-6 and abs(uv1[vertex][1] - 1) < 1e-6
        assert all(abs(a - b) < 1e-6 for a, b in zip(actual_normal, expected_normal)), (name, actual_normal)
    if name == "B":
        assert any(abs(positions[i][2] + position_deltas[i][2] - .5) < 1e-6 for i in indices)
assert checked == {"A", "B"}
print("Independent mixed-attribute GLB skin, UV and morph-normal comparison passed")
