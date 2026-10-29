"""Independently inspect and alter GLB provenance and triangle mappings."""
import copy
import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile


def output(mode):
    result = subprocess.run([sys.argv[1], mode], check=True, text=True, capture_output=True)
    return bytes.fromhex(result.stdout)


def unpack(raw):
    assert struct.unpack_from("<III", raw) == (0x46546C67, 2, len(raw))
    size, kind = struct.unpack_from("<II", raw, 12)
    assert kind == 0x4E4F534A
    document = json.loads(raw[20:20 + size])
    binary_size, kind = struct.unpack_from("<II", raw, 20 + size)
    assert kind == 0x004E4942 and 28 + size + binary_size == len(raw)
    return document, raw[28 + size:]


def pack(document, payload):
    encoded = json.dumps(document, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    binary = bytes(payload) + b"\0" * (-len(payload) % 4)
    return (struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(binary)) +
            struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
            struct.pack("<II", len(binary), 0x004E4942) + binary)


def offset(document, index, element=0):
    accessor = document["accessors"][index]
    view = document["bufferViews"][accessor["bufferView"]]
    components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[accessor["type"]]
    width = {5123: 2, 5125: 4, 5126: 4}[accessor["componentType"]]
    return (view.get("byteOffset", 0) + accessor.get("byteOffset", 0) +
            element * view.get("byteStride", components * width))


baseline, payload = unpack(output("emit_glb_auto"))
assert baseline["asset"]["extras"] == {"iiActorObjectExternalVersion": 1,
                                        "iiActorObjectExternalDocument": "automatic-glb-edit"}
assert baseline["meshes"][0]["extras"]["iiActorObjectSourceMesh"] == 0
assert baseline["materials"][0]["extras"]["iiActorObjectSourceMaterial"] == 0
joint_nodes = baseline["skins"][0]["joints"]
assert [baseline["nodes"][node]["extras"]["iiActorObjectSourceBone"] for node in joint_nodes] == [0, 1, 2]
primitive = baseline["meshes"][0]["primitives"][0]
point_accessor = baseline["accessors"][primitive["attributes"]["_IIACTOROBJECT_POINT_ID"]]
assert point_accessor["componentType"] == 5123 and point_accessor["type"] == "VEC2"
index_accessor = baseline["accessors"][primitive["indices"]]
assert index_accessor["componentType"] == 5125 and index_accessor["count"] == 6

with tempfile.TemporaryDirectory(prefix="glb-external-identity-", dir=os.getcwd()) as directory:
    def inspect(label, mode, document, binary):
        path = pathlib.Path(directory) / (label + ".glb")
        path.write_bytes(pack(document, binary))
        result = subprocess.run([sys.argv[1], mode, str(path)], text=True, capture_output=True)
        if result.returncode:
            raise AssertionError(f"{label}: {result.stdout}\n{result.stderr}")

    document, binary = copy.deepcopy(baseline), bytearray(payload)
    indices = document["meshes"][0]["primitives"][0]["indices"]
    start = offset(document, indices)
    values = list(struct.unpack_from("<6I", binary, start))
    struct.pack_into("<6I", binary, start, *values[4:6], values[3], *values[1:3], values[0])
    inspect("reordered", "inspect_glb_accept", document, binary)

    document, binary = unpack(output("emit_glb_multi"))
    document, binary = copy.deepcopy(document), bytearray(binary)
    assert len(document["meshes"]) == len(document["materials"]) == 2
    root = next(node for node in document["nodes"] if "iiObject3D" in node.get("extras", {}))
    slots = [i for i, node in enumerate(root["children"]) if "mesh" in document["nodes"][node]]
    assert len(slots) == 2
    root["children"][slots[0]], root["children"][slots[1]] = root["children"][slots[1]], root["children"][slots[0]]
    document["materials"].reverse()
    for mesh in document["meshes"]:
        for part in mesh["primitives"]:
            part["material"] = 1 - part["material"]
    inspect("mesh_and_material_reordered", "inspect_glb_multi", document, binary)

    def swap_joint_slots(label, left, right):
        document, binary = copy.deepcopy(baseline), bytearray(payload)
        skin = document["skins"][0]
        skin["joints"][left], skin["joints"][right] = skin["joints"][right], skin["joints"][left]
        matrices = skin["inverseBindMatrices"]
        one, two = offset(document, matrices, left), offset(document, matrices, right)
        old_one, old_two = bytes(binary[one:one + 64]), bytes(binary[two:two + 64])
        binary[one:one + 64], binary[two:two + 64] = old_two, old_one
        joint_accessors = {value for mesh in document["meshes"] for part in mesh["primitives"]
                           for name, value in part["attributes"].items() if name.startswith("JOINTS_")}
        for index in joint_accessors:
            assert document["accessors"][index]["componentType"] == 5123
            for vertex in range(document["accessors"][index]["count"]):
                at = offset(document, index, vertex)
                joints = struct.unpack_from("<4H", binary, at)
                struct.pack_into("<4H", binary, at,
                                 *(right if n == left else left if n == right else n for n in joints))
        inspect(label, "inspect_glb_accept", document, binary)

    swap_joint_slots("joint_slots_reordered", 1, 2)
    swap_joint_slots("root_slot_reordered", 0, 1)

    document, binary = copy.deepcopy(baseline), bytearray(payload)
    attributes = document["meshes"][0]["primitives"][0]["attributes"]
    ids = attributes["_IIACTOROBJECT_POINT_ID"]
    positions = attributes["POSITION"]
    points = document["accessors"][ids]["count"]
    vertex = next(i for i in range(points) if struct.unpack_from("<HH", binary, offset(document, ids, i)) == (2, 0))
    at = offset(document, positions, vertex)
    struct.pack_into("<f", binary, at, struct.unpack_from("<f", binary, at)[0] + .03)
    coordinates = [struct.unpack_from("<fff", binary, offset(document, positions, i)) for i in range(points)]
    document["accessors"][positions]["min"] = [min(p[i] for p in coordinates) for i in range(3)]
    document["accessors"][positions]["max"] = [max(p[i] for p in coordinates) for i in range(3)]
    inspect("edited", "inspect_glb_edit", document, binary)

    for label, change in (
        ("mesh_name", lambda d: next(n for n in d["nodes"] if "mesh" in n).__setitem__("name", "External mesh")),
        ("material_name", lambda d: d["materials"][0].__setitem__("name", "External material")),
        ("bone_name", lambda d: d["nodes"][d["skins"][0]["joints"][1]].__setitem__("name", "Renamed hand")),
    ):
        document, binary = copy.deepcopy(baseline), bytearray(payload)
        change(document)
        inspect(label, "inspect_glb_rename", document, binary)

    cases = (
        ("token_missing", lambda d: d["asset"]["extras"].pop("iiActorObjectExternalDocument")),
        ("token_version", lambda d: d["asset"]["extras"].__setitem__("iiActorObjectExternalVersion", 99)),
        ("mesh_id", lambda d: d["meshes"][0]["extras"].__setitem__("iiActorObjectSourceMesh", 99)),
        ("bone_id", lambda d: d["nodes"][d["skins"][0]["joints"][1]]["extras"].__setitem__("iiActorObjectSourceBone", 0)),
        ("bone_missing", lambda d: d["nodes"][d["skins"][0]["joints"][1]]["extras"].pop("iiActorObjectSourceBone")),
        ("material_id", lambda d: d["materials"][0]["extras"].__setitem__("iiActorObjectSourceMaterial", 99)),
        ("material_missing", lambda d: d["materials"][0]["extras"].pop("iiActorObjectSourceMaterial")),
        ("point_attribute_missing", lambda d: d["meshes"][0]["primitives"][0]["attributes"].pop("_IIACTOROBJECT_POINT_ID")),
        ("bone_renamed_id_missing", lambda d: (d["nodes"][d["skins"][0]["joints"][1]].__setitem__("name", "Renamed hand"),
                                              d["nodes"][d["skins"][0]["joints"][1]]["extras"].pop("iiActorObjectSourceBone"))),
        ("morph_renamed", lambda d: d["meshes"][0]["extras"]["targetNames"].__setitem__(0, "Renamed morph")),
    )
    for label, change in cases:
        document, binary = copy.deepcopy(baseline), bytearray(payload)
        change(document)
        inspect(label, "inspect_glb_reject", document, binary)

    for label, reorder in (("flipped", lambda v: [v[1], v[0], v[2], *v[3:]]),
                           ("duplicate", lambda v: [*v[:3], *v[:3]])):
        document, binary = copy.deepcopy(baseline), bytearray(payload)
        index = document["meshes"][0]["primitives"][0]["indices"]
        start = offset(document, index)
        values = list(struct.unpack_from("<6I", binary, start))
        struct.pack_into("<6I", binary, start, *reorder(values))
        inspect(label, "inspect_glb_reject_file", document, binary)

    document, binary = copy.deepcopy(baseline), bytearray(payload)
    index = document["meshes"][0]["primitives"][0]["indices"]
    start = offset(document, index)
    values = list(struct.unpack_from("<6I", binary, start))
    struct.pack_into("<6I", binary, start, values[1], values[0], values[2], values[4], values[3], values[5])
    inspect("all_triangles_flipped", "inspect_glb_reject", document, binary)

    document, binary = copy.deepcopy(baseline), bytearray(payload)
    index = document["meshes"][0]["primitives"][0]["attributes"]["_IIACTOROBJECT_POINT_ID"]
    struct.pack_into("<HH", binary, offset(document, index), 65535, 65535)
    inspect("point_id", "inspect_glb_reject", document, binary)

    document, binary = copy.deepcopy(baseline), bytearray(payload)
    index = document["meshes"][0]["primitives"][0]["indices"]
    document["accessors"][index]["count"] = 3
    inspect("missing_triangle", "inspect_glb_reject_file", document, binary)

print("Independent GLB provenance, automatic triangle mapping and rejection checks passed")
