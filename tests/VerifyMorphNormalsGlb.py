"""Independently decode sparse GLB NORMAL target accessors using Python's standard library."""
import json
import math
import pathlib
import struct
import sys

raw = pathlib.Path(sys.argv[1]).read_bytes()
magic, version, size = struct.unpack_from("<III", raw)
assert (magic, version, size) == (0x46546C67, 2, len(raw))
json_size, json_type = struct.unpack_from("<II", raw, 12)
assert json_type == 0x4E4F534A
doc = json.loads(raw[20:20 + json_size])
bin_offset = 20 + json_size
bin_size, bin_type = struct.unpack_from("<II", raw, bin_offset)
assert bin_type == 0x004E4942 and bin_offset + 8 + bin_size == len(raw)
binary = memoryview(raw)[bin_offset + 8:]
primitive = doc["meshes"][0]["primitives"][0]
accessor = doc["accessors"][primitive["targets"][0]["NORMAL"]]
assert accessor["componentType"] == 5126 and accessor["type"] == "VEC3" and accessor["count"] == 8
assert "bufferView" not in accessor
sparse = accessor["sparse"]
assert sparse["count"] == 2 and sparse["indices"]["componentType"] == 5125
def view(which):
    entry = sparse[which]
    buffer = doc["bufferViews"][entry["bufferView"]]
    assert buffer.get("buffer", 0) == 0
    return buffer.get("byteOffset", 0) + entry.get("byteOffset", 0)
indices = struct.unpack_from("<2I", binary, view("indices"))
values = struct.unpack_from("<6f", binary, view("values"))
assert indices == (1, 4)
expected = (.2, 0, 0, 0, .2, 0)
assert all(math.isclose(a, b, abs_tol=1e-7) for a, b in zip(values, expected))
print("Independent GLB NORMAL accessor: layouts 1 and 4 preserve distinct sparse deltas")
