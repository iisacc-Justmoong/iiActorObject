"""Inspect GLB bindings independently from the C++ encoder and desktop apps."""
import json
import math
import struct
import subprocess
import sys

raw = bytes.fromhex(subprocess.run([sys.argv[1], "normal_glb", sys.argv[2]], check=True,
                                   text=True, stdout=subprocess.PIPE).stdout.strip())
magic, version, length = struct.unpack_from("<III", raw)
assert (magic, version, length) == (0x46546C67, 2, len(raw))
size, kind = struct.unpack_from("<II", raw, 12)
assert kind == 0x4E4F534A
doc = json.loads(raw[20:20 + size])
bindings = []
for mesh in doc["meshes"]:
    entries = []
    for primitive in mesh["primitives"]:
        material = doc["materials"][primitive["material"]]
        base = material["pbrMetallicRoughness"]["baseColorTexture"]
        normal = material["normalTexture"]
        assert "TEXCOORD_" + str(base["texCoord"]) in primitive["attributes"]
        assert "TEXCOORD_" + str(normal["texCoord"]) in primitive["attributes"]
        assert doc["textures"][normal["index"]]["source"] == 1
        entries.append((material["name"], base["texCoord"], normal["texCoord"], normal["scale"]))
    bindings.append(entries)
assert bindings == [[("Surface", 0, 1, .25), ("Second", 1, 0, -.5)],
                    [("Surface", 0, 2, .25)]]
assert all(math.isfinite(v[3]) for mesh in bindings for v in mesh)
print("Independent GLB normal-map image, per-slot UV and scale passed")
