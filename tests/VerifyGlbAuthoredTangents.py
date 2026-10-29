"""Check authored GLB tangent roundtrips without using the SDK decoder."""
import json
import math
import struct
import subprocess
import sys
import zlib
import copy
import os
import pathlib
import tempfile
from FbxBinaryReader import child, read_fbx


def load(mode):
    raw = bytes.fromhex(subprocess.run([sys.argv[1], mode], check=True,
                                       text=True, capture_output=True).stdout)
    assert struct.unpack_from("<III", raw) == (0x46546C67, 2, len(raw))
    size, kind = struct.unpack_from("<II", raw, 12)
    assert kind == 0x4E4F534A
    document = json.loads(raw[20:20 + size])
    offset = 20 + size
    binary_size, binary_kind = struct.unpack_from("<II", raw, offset)
    assert binary_kind == 0x004E4942 and offset + 8 + binary_size == len(raw)
    return document, raw[offset + 8:]


def accessor(document, binary, index):
    item = document["accessors"][index]
    count = item["count"]
    components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[item["type"]]
    formats = {5121: "B", 5123: "H", 5125: "I", 5126: "f"}
    code = formats[item["componentType"]]
    width = struct.calcsize("<" + code)
    values = [[0] * components for _ in range(count)]
    if "bufferView" in item:
        view = document["bufferViews"][item["bufferView"]]
        start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
        stride = view.get("byteStride", components * width)
        for i in range(count):
            values[i] = list(struct.unpack_from("<" + code * components, binary, start + stride * i))
    if "sparse" in item:
        sparse = item["sparse"]
        indices = sparse["indices"]
        iview = document["bufferViews"][indices["bufferView"]]
        icode = formats[indices["componentType"]]
        istart = iview.get("byteOffset", 0) + indices.get("byteOffset", 0)
        value = sparse["values"]
        vview = document["bufferViews"][value["bufferView"]]
        vstart = vview.get("byteOffset", 0) + value.get("byteOffset", 0)
        for i in range(sparse["count"]):
            target = struct.unpack_from("<" + icode, binary, istart + i * struct.calcsize("<" + icode))[0]
            values[target] = list(struct.unpack_from("<" + code * components, binary,
                                                      vstart + i * width * components))
    return values


def normalized(values):
    length = math.sqrt(sum(x*x for x in values))
    return [x / length for x in values]


def close(a, b):
    return math.isclose(a, b, abs_tol=2e-6)


source, binary = load("source")
original = source["meshes"][0]["primitives"][0]
source_t = accessor(source, binary, original["attributes"]["TANGENT"])[0]
source_delta = accessor(source, binary, original["targets"][0]["TANGENT"])[0]
scale = source["nodes"][0]["scale"]
expected_tangent = normalized([source_t[i] * scale[i] for i in range(3)])
expected_morphed = normalized([(source_t[i] + .25 * source_delta[i]) * scale[i] for i in range(3)])

for mode in ("export", "project", "undo"):
    doc, binary = load(mode)
    primitives = doc["meshes"][0]["primitives"]
    assert len(primitives) == 2
    authored = [p for p in primitives if "TANGENT" in p["attributes"]]
    assert len(authored) == 1, "The primitive without source tangents must remain separate"
    primitive = authored[0]
    tangent = accessor(doc, binary, primitive["attributes"]["TANGENT"])
    delta = accessor(doc, binary, primitive["targets"][0]["TANGENT"])
    indices = [value[0] for value in accessor(doc, binary, primitive["indices"])]
    assert len(indices) == 3
    for i in indices:
        assert all(close(a, b) for a, b in zip(tangent[i][:3], expected_tangent))
        assert tangent[i][3] == 1, "The reflected transform must flip handedness"
        morphed = normalized([tangent[i][j] + .25 * delta[i][j] for j in range(3)])
        assert all(close(a, b) for a, b in zip(morphed, expected_morphed))

doc, binary = load("edited")
primitives = doc["meshes"][0]["primitives"]
assert all("TANGENT" not in p["attributes"] for p in primitives)
assert any(p.get("extras", {}).get("iiActorObjectAuthoredTangentStatus") == "source_tangents_stale"
           for p in primitives)
for primitive in primitives:
    positions = accessor(doc, binary, primitive["attributes"]["POSITION"])
    indices = [item[0] for item in accessor(doc, binary, primitive["indices"])]
    for triangle in range(0, len(indices), 3):
        assert any(close(positions[i][0], .1) and close(positions[i][1], 0) and close(positions[i][2], 0)
                   for i in indices[triangle:triangle + 3]), "A shared source point must move on both primitive sides"

doc, binary = load("flat_export")
flat = doc["meshes"][0]["primitives"]
assert len(flat) == 1
primitive = flat[0]
normals = accessor(doc, binary, primitive["attributes"]["NORMAL"])
deltas = accessor(doc, binary, primitive["targets"][0]["NORMAL"])
indices = [item[0] for item in accessor(doc, binary, primitive["indices"])]
assert len(indices) == 6 and len(set(indices)) == 6
for corner, index in enumerate(indices):
    normal = normals[index]
    expected = [0, 0, 1] if corner < 3 else [0, 1, 0]
    assert all(close(a, b) for a, b in zip(normal, expected))
    target = [normal[j] + deltas[index][j] for j in range(3)]
    expected = [0, -1 / math.sqrt(37), 6 / math.sqrt(37)] if corner < 3 else [0, 1, 0]
    assert all(close(a, b) for a, b in zip(target, expected))

for mode, expected_version in (("archive", 29), ("archive_v24", 24)):
    archive = bytes.fromhex(subprocess.run([sys.argv[1], mode], check=True,
                                          text=True, capture_output=True).stdout)
    magic, version, size, checksum = struct.unpack_from("<8sIQI", archive)
    assert magic == b"iiAOPRJ\0" and version == expected_version and size == len(archive) - 24
    assert zlib.crc32(archive[24:]) == checksum
print("Independent authored tangents, morph, reflection, flat normals, edit and project checks passed")


def fbx(mode):
    data = bytes.fromhex(subprocess.run([sys.argv[1], mode], check=True,
                                         text=True, capture_output=True).stdout)
    objects = next(node for node in read_fbx(data) if node[0] == "Objects")[2]
    mesh = next(node for node in objects if node[0] == "Geometry" and node[1][2] == "Mesh")
    tangent = {node[1][0]: node for node in mesh[2] if node[0] == "LayerElementTangent"}
    binormal = {node[1][0]: node for node in mesh[2] if node[0] == "LayerElementBinormal"}
    status = [child(node, "Reason")[1][0] for node in mesh[2]
              if node[0] == "iiActorObjectTangentStatus"]
    return tangent, binormal, status


for mode in ("fbx_source", "fbx_undo", "fbx_project"):
    tangents, binormals, statuses = fbx(mode)
    assert set(tangents) == set(binormals) == {0}
    values = child(tangents[0], "Tangents")[1][0]
    signs = child(tangents[0], "TangentsW")[1][0]
    bitangents = child(binormals[0], "Binormals")[1][0]
    assert len(values) == 18 and len(signs) == 6
    for corner in range(3):
        assert all(close(a, b) for a, b in zip(values[3*corner:3*corner+3], expected_tangent))
        assert signs[corner] == -1, "FBX uses the opposite texture V orientation"
        expected_bitangent = [expected_tangent[1], -expected_tangent[0], 0]
        assert all(close(a, b) for a, b in zip(bitangents[3*corner:3*corner+3], expected_bitangent))
    assert any(not close(a, b) for a, b in zip(values[9:12], expected_tangent)), \
        "A second primitive without authored tangents must keep its calculated basis"
    assert "base_only_morphs" in statuses

for mode in ("fbx_all", "fbx_degenerate"):
    tangents, binormals, statuses = fbx(mode)
    assert set(tangents) == set(binormals) == {0}
    values = child(tangents[0], "Tangents")[1][0]
    signs = child(tangents[0], "TangentsW")[1][0]
    assert len(values) == 18 and signs == (-1,) * 6
    for corner in range(6):
        assert all(close(a, b) for a, b in zip(values[3*corner:3*corner+3], expected_tangent))
    assert "base_only_morphs" in statuses

tangents, binormals, statuses = fbx("fbx_uv1")
assert set(tangents) == set(binormals) == {1}
values = child(tangents[1], "Tangents")[1][0]
signs = child(tangents[1], "TangentsW")[1][0]
for corner in range(3):
    assert all(close(a, b) for a, b in zip(values[3*corner:3*corner+3], expected_tangent))
    assert signs[corner] == -1
assert "base_only_morphs" in statuses

tangents, _, statuses = fbx("fbx_edited")
assert "source_tangents_stale" in statuses
assert 0 in tangents and any(not close(a, b) for a, b in
                             zip(child(tangents[0], "Tangents")[1][0][:3], expected_tangent))
tangents, _, statuses = fbx("fbx_remapped")
assert "source_tangent_uv_changed" in statuses
assert set(tangents) == {1}
assert any(not close(a, b) for a, b in zip(child(tangents[1], "Tangents")[1][0][:3], expected_tangent))
tangents, binormals, statuses = fbx("fbx_unbound")
assert not tangents and not binormals and "source_tangent_uv_unbound" in statuses
print("Independent GLB to FBX authored and calculated basis checks passed")


def pack_glb(document, payload):
    encoded = json.dumps(document, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    payload = bytes(payload) + b"\0" * (-len(payload) % 4)
    return (struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(payload)) +
            struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
            struct.pack("<II", len(payload), 0x004E4942) + payload)


def attribute_offset(document, index, element):
    item = document["accessors"][index]
    view = document["bufferViews"][item["bufferView"]]
    components = {"VEC2": 2, "VEC3": 3, "VEC4": 4}[item["type"]]
    width = {5121: 1, 5123: 2, 5126: 4}[item["componentType"]]
    return view.get("byteOffset", 0) + item.get("byteOffset", 0) + element * view.get("byteStride", components * width)


def point_id(value):
    return value[0] | (value[1] << 16)


baseline, baseline_binary = load("export")
mesh = baseline["meshes"][0]
identity = mesh["extras"]["iiActorObjectPoints"]
assert identity["version"] == 1
view = baseline["bufferViews"][identity["bufferView"]]
begin = view.get("byteOffset", 0)
assert baseline_binary[begin:begin + 8] == b"iiGLPT\0\0"
version, point_count, target_count, unused_count, morph_count, _, bone_count = \
    struct.unpack_from("<7I", baseline_binary, begin + 8)
assert (version, point_count, target_count, unused_count, morph_count, bone_count) == (1, 4, 1, 1, 1, 0)
for primitive in mesh["primitives"]:
    ids = accessor(baseline, baseline_binary, primitive["attributes"]["_IIACTOROBJECT_POINT_ID"])
    assert all(point_id(value) < 4 for value in ids)

with tempfile.TemporaryDirectory(prefix="glb-point-identity-", dir=os.getcwd()) as directory:
    def inspect(mode, document, payload):
        path = pathlib.Path(directory) / (mode + ".glb")
        path.write_bytes(pack_glb(document, payload))
        result = subprocess.run([sys.argv[1], mode, str(path)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(f"{mode} failed: {result.stdout}\n{result.stderr}")

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    changed = 0
    for primitive in doc["meshes"][0]["primitives"]:
        position = primitive["attributes"]["POSITION"]
        ids = accessor(doc, binary, primitive["attributes"]["_IIACTOROBJECT_POINT_ID"])
        visible = {value[0] for value in accessor(doc, binary, primitive["indices"])}
        for i, value in enumerate(ids):
            if i in visible and point_id(value) == 0:
                struct.pack_into("<f", binary, attribute_offset(doc, position, i), .125)
                changed += 1
        coordinates = accessor(doc, binary, position)
        doc["accessors"][position]["min"] = [min(p[i] for p in coordinates) for i in range(3)]
        doc["accessors"][position]["max"] = [max(p[i] for p in coordinates) for i in range(3)]
    assert changed >= 2
    inspect("inspect_consistent", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    changed = set()
    for primitive in doc["meshes"][0]["primitives"]:
        ids = accessor(doc, binary, primitive["attributes"]["_IIACTOROBJECT_POINT_ID"])
        visible = {value[0] for value in accessor(doc, binary, primitive["indices"])}
        morph_id = primitive["targets"][0]["POSITION"]
        morph = doc["accessors"][morph_id]
        sparse = morph["sparse"]
        iv = doc["bufferViews"][sparse["indices"]["bufferView"]]
        code = {5121: "B", 5123: "H", 5125: "I"}[sparse["indices"]["componentType"]]
        start = iv.get("byteOffset", 0) + sparse["indices"].get("byteOffset", 0)
        width = struct.calcsize("<" + code)
        values = doc["bufferViews"][sparse["values"]["bufferView"]]
        value_start = values.get("byteOffset", 0) + sparse["values"].get("byteOffset", 0)
        for entry in range(sparse["count"]):
            vertex = struct.unpack_from("<" + code, binary, start + entry * width)[0]
            if vertex in visible and point_id(ids[vertex]) == 0:
                offset = value_start + entry * 12
                struct.pack_into("<f", binary, offset + 8, .3)
                changed.add(offset)
        coordinates = accessor(doc, binary, morph_id)
        morph["min"] = [min(p[i] for p in coordinates) for i in range(3)]
        morph["max"] = [max(p[i] for p in coordinates) for i in range(3)]
    assert len(changed) >= 2
    inspect("inspect_morph_consistent", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    primitive = next(p for p in doc["meshes"][0]["primitives"] if "TANGENT" in p["attributes"])
    ids = accessor(doc, binary, primitive["attributes"]["_IIACTOROBJECT_POINT_ID"])
    indices = [value[0] for value in accessor(doc, binary, primitive["indices"])]
    index = next(i for i in indices if point_id(ids[i]) == 0)
    position = primitive["attributes"]["POSITION"]
    struct.pack_into("<f", binary, attribute_offset(doc, position, index), .125)
    coordinates = accessor(doc, binary, position)
    doc["accessors"][position]["min"] = [min(p[i] for p in coordinates) for i in range(3)]
    doc["accessors"][position]["max"] = [max(p[i] for p in coordinates) for i in range(3)]
    inspect("inspect_conflict", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    primitive = next(p for p in doc["meshes"][0]["primitives"] if "TANGENT" in p["attributes"])
    ids = accessor(doc, binary, primitive["attributes"]["_IIACTOROBJECT_POINT_ID"])
    indices = [value[0] for value in accessor(doc, binary, primitive["indices"])]
    vertex = next(i for i in indices if point_id(ids[i]) == 0)
    morph = doc["accessors"][primitive["targets"][0]["POSITION"]]
    sparse = morph["sparse"]
    iv = doc["bufferViews"][sparse["indices"]["bufferView"]]
    code = {5121: "B", 5123: "H", 5125: "I"}[sparse["indices"]["componentType"]]
    start = iv.get("byteOffset", 0) + sparse["indices"].get("byteOffset", 0)
    width = struct.calcsize("<" + code)
    entry = next(i for i in range(sparse["count"]) if struct.unpack_from("<" + code, binary, start + i * width)[0] == vertex)
    vv = doc["bufferViews"][sparse["values"]["bufferView"]]
    offset = vv.get("byteOffset", 0) + sparse["values"].get("byteOffset", 0) + entry * 12
    struct.pack_into("<f", binary, offset + 8, .3)
    morph["min"], morph["max"] = [-1, -1, -1], [1, 1, 1]
    inspect("inspect_morph_conflict", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    del doc["meshes"][0]["primitives"][0]["attributes"]["_IIACTOROBJECT_POINT_ID"]
    inspect("inspect_missing", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    primitive = doc["meshes"][0]["primitives"][0]
    ids = primitive["attributes"]["_IIACTOROBJECT_POINT_ID"]
    struct.pack_into("<HH", binary, attribute_offset(doc, ids, 0), 65535, 65535)
    inspect("inspect_badid", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    identity = doc["meshes"][0]["extras"]["iiActorObjectPoints"]
    view = doc["bufferViews"][identity["bufferView"]]
    binary[view.get("byteOffset", 0)] ^= 0xff
    inspect("inspect_malformed", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    identity = doc["meshes"][0]["extras"]["iiActorObjectPoints"]
    view = doc["bufferViews"][identity["bufferView"]]
    struct.pack_into("<I", binary, view.get("byteOffset", 0) + 8, 99)
    inspect("inspect_version", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    identity = doc["meshes"][0]["extras"]["iiActorObjectPoints"]
    view = doc["bufferViews"][identity["bufferView"]]
    struct.pack_into("<I", binary, view.get("byteOffset", 0) + 20, 0xffffffff)
    inspect("inspect_count", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    identity = doc["meshes"][0]["extras"]["iiActorObjectPoints"]
    doc["bufferViews"][identity["bufferView"]]["byteLength"] -= 1
    inspect("inspect_truncated", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    del doc["meshes"][0]["extras"]["iiActorObjectPoints"]
    inspect("inspect_orphan", doc, binary)

    doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
    doc["meshes"][0]["extras"]["targetNames"][0] = "Renamed"
    inspect("inspect_renamed", doc, binary)

    for mode, field, value in (("inspect_transform", "translation", [1, 0, 0]),
                               ("inspect_reflection", "scale", [-1, 1, 1])):
        doc, binary = copy.deepcopy(baseline), bytearray(baseline_binary)
        node = next(node for node in doc["nodes"] if node.get("mesh") == 0)
        node[field] = value
        inspect(mode, doc, binary)

    skin, skin_binary = load("skin_export")
    for mode, all_occurrences, selected_id, joint in (("inspect_skin_consistent", True, 0, 0),
                                                       ("inspect_skin_conflict", False, 0, 0),
                                                       ("inspect_skin_empty_changed", True, 2, 1)):
        doc, binary = copy.deepcopy(skin), bytearray(skin_binary)
        changed = set()
        for primitive in doc["meshes"][0]["primitives"]:
            ids = accessor(doc, binary, primitive["attributes"]["_IIACTOROBJECT_POINT_ID"])
            indices = {value[0] for value in accessor(doc, binary, primitive["indices"])}
            joints = primitive["attributes"]["JOINTS_0"]
            code = {5121: "B", 5123: "H"}[doc["accessors"][joints]["componentType"]]
            for index in indices:
                if point_id(ids[index]) != selected_id:
                    continue
                offset = attribute_offset(doc, joints, index)
                if offset in changed or (changed and not all_occurrences):
                    continue
                struct.pack_into("<" + code, binary, offset, joint)
                changed.add(offset)
        assert len(changed) == (2 if all_occurrences else 1)
        inspect(mode, doc, binary)

    doc, binary = copy.deepcopy(skin), bytearray(skin_binary)
    node = doc["skins"][0]["joints"][1]
    doc["nodes"][node]["name"] = "RenamedJaw"
    inspect("inspect_skin_renamed", doc, binary)
print("Independent GLB source point identity, external edits and fallback checks passed")
