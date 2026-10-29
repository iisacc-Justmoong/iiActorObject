"""Check authored tangent retention with an independent binary FBX reader."""
import subprocess
import sys
import os
import pathlib
import struct
import tempfile
import zlib
import json
import math

from FbxBinaryReader import child, read_fbx


def layers(mode):
    output = subprocess.run([sys.argv[1], mode, sys.argv[2]], check=True,
                            text=True, capture_output=True).stdout
    root = read_fbx(bytes.fromhex(output))
    objects = next(n for n in root if n[0] == "Objects")[2]
    mesh = next(n for n in objects if n[0] == "Geometry" and n[1][2] == "Mesh")
    tangents = {n[1][0]: n for n in mesh[2] if n[0] == "LayerElementTangent"}
    bitangents = {n[1][0]: n for n in mesh[2] if n[0] == "LayerElementBinormal"}
    return mesh, tangents, bitangents


for mode in ("roundtrip", "undo", "project", "material", "layered", "hidden"):
    mesh, t, b = layers(mode)
    assert set(t) == set(b) == {0, 1}, (mode, t.keys(), b.keys())
    assert child(t[0], "Tangents")[1][0] == (0,1,0)*3
    assert child(t[1], "Tangents")[1][0] == (0,-1,0)*3
    assert child(t[0], "TangentsW")[1][0] == (1,)*3
    assert child(t[1], "TangentsW")[1][0] == (-1,)*3
    assert child(b[0], "Binormals")[1][0] == (-1,0,0)*3
    assert child(b[1], "Binormals")[1][0] == (-1,0,0)*3
    assert child(b[0], "BinormalsW")[1][0] == (.25,)*3
    assert child(b[1], "BinormalsW")[1][0] == (.5,)*3

for mode in ("edited", "position", "normal", "detail_uv"):
    mesh, t, b = layers(mode)
    assert 0 not in t and 0 not in b
    assert 1 in t and 1 in b
    assert child(t[1], "Tangents")[1][0] != (0,-1,0)*3
    statuses = [child(n, "Reason")[1][0] for n in mesh[2] if n[0] == "iiActorObjectTangentStatus"]
    assert statuses.count("source_tangents_stale") == 2

mesh, t, b = layers("reflection")
assert child(t[0], "Tangents")[1][0] == (0,1,0)*3
assert child(t[1], "Tangents")[1][0] == (0,-1,0)*3
assert child(b[0], "Binormals")[1][0] == (1,0,0)*3
assert child(b[1], "Binormals")[1][0] == (1,0,0)*3
assert child(t[0], "TangentsW")[1][0] == (-1,)*3
assert child(t[1], "TangentsW")[1][0] == (1,)*3

mesh, t, b = layers("nonuniform")
assert child(t[0], "Tangents")[1][0] == (0,3,0)*3
assert child(t[1], "Tangents")[1][0] == (0,-3,0)*3
assert child(b[0], "Binormals")[1][0] == (-2,0,0)*3
assert child(b[1], "Binormals")[1][0] == (-2,0,0)*3

mesh, t, b = layers("tangent_only")
assert set(t) == {0, 1} and not b
mesh, t, b = layers("binormal_only")
assert not t and set(b) == {0, 1}
mesh, t, b = layers("no_w")
assert set(t) == set(b) == {0, 1}
assert all(not any(n[0] == "TangentsW" for n in layer[2]) for layer in t.values())
assert all(not any(n[0] == "BinormalsW" for n in layer[2]) for layer in b.values())
subprocess.run([sys.argv[1], "validation", sys.argv[2]], check=True, capture_output=True)
raw = bytes.fromhex(subprocess.run([sys.argv[1], "project_bytes", sys.argv[2]], check=True,
                                  text=True, capture_output=True).stdout)
magic, version, size, checksum = struct.unpack_from("<8sIQI", raw)
assert magic == b"iiAOPRJ\0" and version == 20 and size == len(raw)-24
assert zlib.crc32(raw[24:]) == checksum
# The two editor-history assets share one source snapshot in the archive.
needle = struct.pack("<Q9d", 3, *(0,1,0)*3)
assert raw.count(needle) == 1
invalid_value = bytearray(raw)
struct.pack_into("<d", invalid_value, invalid_value.index(needle)+8, float("nan"))
struct.pack_into("<I", invalid_value, 20, zlib.crc32(invalid_value[24:]))
unknown_version = bytearray(raw)
struct.pack_into("<I", unknown_version, 8, 26)
with tempfile.TemporaryDirectory(prefix="authored-tangent-project-", dir=os.getcwd()) as directory:
    path = pathlib.Path(directory) / "snapshot.ii3dproject"
    for data, valid in ((raw, True), (invalid_value, False), (unknown_version, False)):
        path.write_bytes(data)
        result = subprocess.run([sys.argv[1], "read_project", str(path)], capture_output=True)
        assert (result.returncode == 0) == valid
print("Independent authored FBX tangent roundtrip, edit, undo and project checks passed")


def glb(mode):
    raw = bytes.fromhex(subprocess.run([sys.argv[1], "glb_" + mode, sys.argv[2]], check=True,
                                      text=True, capture_output=True).stdout)
    assert struct.unpack_from("<III", raw) == (0x46546C67, 2, len(raw))
    size, kind = struct.unpack_from("<II", raw, 12)
    assert kind == 0x4E4F534A
    document = json.loads(raw[20:20 + size])
    length, kind = struct.unpack_from("<II", raw, 20 + size)
    assert kind == 0x004E4942 and 28 + size + length == len(raw)
    return document, raw[28 + size:]


def accessor(document, binary, index):
    item = document["accessors"][index]
    components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[item["type"]]
    code = {5121: "B", 5123: "H", 5125: "I", 5126: "f"}[item["componentType"]]
    width = struct.calcsize("<" + code)
    view = document["bufferViews"][item["bufferView"]]
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    stride = view.get("byteStride", components * width)
    return [struct.unpack_from("<" + code * components, binary, start + stride*i)
            for i in range(item["count"])]


def check_glb(mode, expected, sign, uv=1, count=3):
    document, binary = glb(mode)
    primitive = document["meshes"][0]["primitives"][0]
    material = document["materials"][primitive["material"]]
    if "normalTexture" in material:
        assert material["normalTexture"].get("texCoord", 0) == uv
    tangents = accessor(document, binary, primitive["attributes"]["TANGENT"])
    indices = [v[0] for v in accessor(document, binary, primitive["indices"])]
    assert len(indices) == count
    for index in indices:
        tangent = tangents[index]
        assert all(math.isclose(a, b, abs_tol=2e-6) for a, b in zip(tangent[:3], expected)), (mode, tangent)
        assert tangent[3] == sign, (mode, tangent)
    return primitive, tangents, indices


for mode in ("source", "undo", "project", "no_w", "nonuniform"):
    primitive, _, _ = check_glb(mode, (0, -1, 0), 1)
    if mode != "no_w":
        assert "source_fbx_binormal_w_omitted" in primitive["extras"]["iiActorObjectFbxTangentNotes"]
    assert "source_fbx_other_uv_layers_omitted" in primitive["extras"]["iiActorObjectFbxTangentNotes"]
check_glb("remap", (0, 1, 0), -1, uv=0)
check_glb("reflection", (0, -1, 0), -1)
check_glb("quad", (0, -1, 0), 1, count=6)

primitive, _, _ = check_glb("disagree", (0, -1, 0), 1)
assert "source_fbx_tangent_w_conflict" in primitive["extras"]["iiActorObjectFbxTangentNotes"]
primitive, _, _ = check_glb("approx", (0, -1, 0), 1)
assert "source_fbx_binormal_approximated" in primitive["extras"]["iiActorObjectFbxTangentNotes"]
primitive, _, _ = check_glb("one_unbound", (0, -1, 0), 1)
assert primitive["extras"]["iiActorObjectFbxTangentUvSet"] == 1
assert "source_fbx_unbound_uv" in primitive["extras"]["iiActorObjectFbxTangentNotes"]
primitive, _, _ = check_glb("morph", (0, -1, 0), 1)
assert "source_fbx_morph_tangents_unavailable" in primitive["extras"]["iiActorObjectFbxTangentNotes"]
assert len(primitive["targets"]) == 1 and "TANGENT" not in primitive["targets"][0]

document, binary = glb("seams")
primitive = document["meshes"][0]["primitives"][0]
tangents = accessor(document, binary, primitive["attributes"]["TANGENT"])
positions = accessor(document, binary, primitive["attributes"]["POSITION"])
indices = [v[0] for v in accessor(document, binary, primitive["indices"])]
assert len(indices) == 6
assert {tuple(round(v, 5) for v in tangents[index][:3]) for index in indices} == {(0, -1, 0), (1, 0, 0)}
assert len({tuple(positions[index]) for index in indices}) == 3, "The authored seam shares source points"

document, binary = glb("multi_material")
primitives = document["meshes"][0]["primitives"]
assert len(primitives) == 2
seen = set()
for primitive in primitives:
    material = document["materials"][primitive["material"]]
    uv = material["normalTexture"].get("texCoord", 0)
    assert uv in (0, 1) and uv not in seen
    seen.add(uv)
    values = accessor(document, binary, primitive["attributes"]["TANGENT"])
    indices = [v[0] for v in accessor(document, binary, primitive["indices"])]
    expected = (0, 1, 0, -1) if uv == 0 else (0, -1, 0, 1)
    assert len(indices) == 3
    for index in indices:
        assert all(math.isclose(a, b, abs_tol=2e-6) for a, b in zip(values[index], expected))
assert seen == {0, 1}

document, binary = glb("mixed_basis")
primitives = document["meshes"][0]["primitives"]
assert len(primitives) == 2, "A bad basis on one face must not discard another face's authored tangent"
authored = fallback = None
for primitive in primitives:
    notes = primitive.get("extras", {}).get("iiActorObjectFbxTangentNotes", [])
    if "source_fbx_handedness_unknown" in notes:
        fallback = primitive
    else:
        authored = primitive
assert authored is not None and fallback is not None
for primitive, expected in ((authored, (0, -1, 0, 1)), (fallback, (1, 0, 0, -1))):
    values = accessor(document, binary, primitive["attributes"]["TANGENT"])
    indices = [v[0] for v in accessor(document, binary, primitive["indices"])]
    assert len(indices) == 3
    for index in indices:
        assert all(math.isclose(a, b, abs_tol=2e-6) for a, b in zip(values[index], expected))

document, binary = glb("mixed_basis_unbound")
primitives = document["meshes"][0]["primitives"]
assert len(primitives) == 2
authored = [p for p in primitives if "TANGENT" in p["attributes"]]
fallback = [p for p in primitives if "TANGENT" not in p["attributes"]]
assert len(authored) == len(fallback) == 1
for primitive in primitives:
    assert primitive["extras"]["iiActorObjectFbxTangentUvSet"] == 1
    assert "source_fbx_unbound_uv" in primitive["extras"]["iiActorObjectFbxTangentNotes"]
assert "source_fbx_handedness_unknown" in fallback[0]["extras"]["iiActorObjectFbxTangentNotes"]

for mode, note in (("edited", "source_fbx_tangents_stale"),
                   ("position", "source_fbx_tangents_stale"),
                   ("normal", "source_fbx_tangents_stale"),
                   ("tangent_only", "source_fbx_handedness_unknown"),
                   ("binormal_only", "source_fbx_tangent_missing"),
                   ("zero_binormal", "source_fbx_handedness_unknown"),
                   ("mixed_handedness", "source_fbx_triangle_handedness"),
                   ("missing_layer", "source_fbx_uv_layer_missing")):
    document, binary = glb(mode)
    primitive = document["meshes"][0]["primitives"][0]
    assert note in primitive["extras"]["iiActorObjectFbxTangentNotes"], (mode, primitive.get("extras"))
    assert "TANGENT" in primitive["attributes"], "A normal map should use the computed fallback"

document, _ = glb("unbound")
primitive = document["meshes"][0]["primitives"][0]
assert "TANGENT" not in primitive["attributes"]
assert "source_fbx_uv_ambiguous" in primitive["extras"]["iiActorObjectFbxTangentNotes"]

mesh, t, b = layers("glb_via_fbx")
assert set(t) == set(b) == {1}
for value, expected in zip(child(t[1], "Tangents")[1][0], (0, -1, 0) * 3):
    assert math.isclose(value, expected, abs_tol=2e-6)
assert child(t[1], "TangentsW")[1][0] == (-1,) * 3
print("Independent FBX to GLB tangents, seams, UV selection, fallback and roundtrip checks passed")
