"""Read a generated GLB independently, without running a desktop application."""
import json
import math
import struct
import subprocess
import sys

raw = bytes.fromhex(subprocess.run([sys.argv[1], "multi_uv_glb", sys.argv[2]], check=True,
                                   text=True, stdout=subprocess.PIPE).stdout.strip())
magic, version, length = struct.unpack_from("<III", raw)
assert (magic, version, length) == (0x46546C67, 2, len(raw))
json_length, json_type = struct.unpack_from("<II", raw, 12)
assert json_type == 0x4E4F534A
document = json.loads(raw[20:20 + json_length])
offset = 20 + json_length
binary_length, binary_type = struct.unpack_from("<II", raw, offset)
assert binary_type == 0x004E4942 and offset + 8 + binary_length == len(raw)
binary = raw[offset + 8:]


def accessor(index):
    item = document["accessors"][index]
    view = document["bufferViews"][item["bufferView"]]
    assert item["componentType"] == 5126 and view["buffer"] == 0
    width = {"VEC2": 2, "VEC3": 3}[item["type"]]
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    values = struct.unpack_from("<" + "f" * (item["count"] * width), binary, start)
    return [values[i:i + width] for i in range(0, len(values), width)]


attributes = document["meshes"][0]["primitives"][0]["attributes"]
primitives = document["meshes"][0]["primitives"]
assert len(primitives) == 2
assert {document["materials"][p["material"]]["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"]
        for mesh in document["meshes"] for p in mesh["primitives"]} == {0, 1, 2}
bindings = []
for mesh in document["meshes"]:
    bindings.append([(document["materials"][p["material"]]["name"],
                      document["materials"][p["material"]]["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"])
                     for p in mesh["primitives"]])
assert bindings == [[("Surface", 1), ("Second", 0)], [("Surface", 2)]]
positions = accessor(attributes["POSITION"])
surface = accessor(attributes["TEXCOORD_0"])
lightmap = accessor(attributes["TEXCOORD_1"])
assert len(positions) == len(surface) == len(lightmap) == 6
assert positions[0] == positions[3] and surface[0] == surface[3]
assert lightmap[0] != lightmap[3]
expected = [(0, 1), (.4, 1), (.4, .6), (.6, .4), (1, 0), (.6, 0)]
for actual, want in zip(lightmap, expected):
    assert all(math.isclose(a, b, abs_tol=1e-6) for a, b in zip(actual, want))
second_attributes = document["meshes"][1]["primitives"][0]["attributes"]
assert accessor(second_attributes["TEXCOORD_2"]) == lightmap
print("independent GLB multiple-UV data passed")
