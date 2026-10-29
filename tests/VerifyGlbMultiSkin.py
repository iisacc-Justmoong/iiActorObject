"""Compare source and merged GLB skinning without using the SDK's decoder."""
import json
import functools
import math
import struct
import subprocess
import sys


def fixture(mode):
    result = subprocess.run([sys.argv[1], mode], check=True, text=True, capture_output=True)
    raw = bytes.fromhex(result.stdout.strip())
    magic, version, size = struct.unpack_from("<III", raw)
    assert (magic, version, size) == (0x46546C67, 2, len(raw))
    json_size, json_tag = struct.unpack_from("<II", raw, 12)
    assert json_tag == 0x4E4F534A
    document = json.loads(raw[20:20 + json_size])
    offset = 20 + json_size
    binary_size, binary_tag = struct.unpack_from("<II", raw, offset)
    assert binary_tag == 0x004E4942 and offset + 8 + binary_size == len(raw)
    return document, raw[offset + 8:]


def accessor(document, binary, index):
    item = document["accessors"][index]
    assert "sparse" not in item
    view = document["bufferViews"][item["bufferView"]]
    component = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}[item["componentType"]]
    width = {"SCALAR": 1, "VEC3": 3, "VEC4": 4, "MAT4": 16}[item["type"]]
    stride = view.get("byteStride", width * component[1])
    offset = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    values = []
    for i in range(item["count"]):
        value = struct.unpack_from("<" + component[0] * width, binary, offset + i * stride)
        if item.get("normalized", False):
            divisor = {5121: 255, 5123: 65535}[item["componentType"]]
            value = tuple(x / divisor for x in value)
        values.append(value)
    return values


def point(matrix, vertex):
    source = (*vertex, 1.0)
    return tuple(sum(matrix[column * 4 + row] * source[column] for column in range(4)) for row in range(3))


def multiply(a, b):
    return tuple(sum(a[k * 4 + row] * b[column * 4 + k] for k in range(4))
                 for column in range(4) for row in range(4))


def evaluated_meshes(document, binary):
    nodes = document["nodes"]
    parents = {}
    for i, node in enumerate(nodes):
        for child in node.get("children", []):
            assert child not in parents
            parents[child] = i

    @functools.lru_cache(maxsize=None)
    def world_matrix(index):
        node = nodes[index]
        assert "matrix" not in node
        x, y, z, w = node.get("rotation", [0, 0, 0, 1])
        scale = node.get("scale", [1, 1, 1])
        translation = list(node.get("translation", [0, 0, 0]))
        if node.get("name") == "Child":
            translation[0] += 1
        if node.get("name") == "OtherRoot":
            assert (x, y, z, w) == (0, 0, 0, 1)
            z = w = math.sqrt(.5)
            translation[1] += 1
        local = (
            (1 - 2 * (y*y + z*z))*scale[0], (2 * (x*y + z*w))*scale[0], (2 * (x*z - y*w))*scale[0], 0,
            (2 * (x*y - z*w))*scale[1], (1 - 2 * (x*x + z*z))*scale[1], (2 * (y*z + x*w))*scale[1], 0,
            (2 * (x*z + y*w))*scale[2], (2 * (y*z - x*w))*scale[2], (1 - 2 * (x*x + y*y))*scale[2], 0,
            *translation, 1,
        )
        if index in parents:
            local = multiply(world_matrix(parents[index]), local)
        return local

    result = {}
    for node_index, node in enumerate(nodes):
        if "mesh" not in node:
            continue
        mesh = document["meshes"][node["mesh"]]
        assert len(mesh["primitives"]) == 1
        primitive = mesh["primitives"][0]
        attributes = primitive["attributes"]
        positions = accessor(document, binary, attributes["POSITION"])
        indices = [value[0] for value in accessor(document, binary, primitive["indices"])]
        if "skin" not in node:
            world = world_matrix(node_index)
            result[node["name"]] = [point(world, positions[i]) for i in indices]
            continue
        skin = document["skins"][node["skin"]]
        matrices = accessor(document, binary, skin["inverseBindMatrices"])
        joints = accessor(document, binary, attributes["JOINTS_0"])
        weights = accessor(document, binary, attributes["WEIGHTS_0"])
        vertices = []
        for vertex in indices:
            value = [0.0, 0.0, 0.0]
            for joint, weight in zip(joints[vertex], weights[vertex]):
                if weight == 0:
                    continue
                transformed = point(multiply(world_matrix(skin["joints"][joint]), matrices[joint]), positions[vertex])
                for axis in range(3):
                    value[axis] += weight * transformed[axis]
            vertices.append(tuple(value))
        result[node["name"]] = vertices
    return result


source, source_binary = fixture("multiskin_source")
exported, exported_binary = fixture("multiskin_export")
assert len(source["skins"]) == 2 and len(exported["skins"]) == 1
before = evaluated_meshes(source, source_binary)
after = evaluated_meshes(exported, exported_binary)
assert set(before) == set(after) == {"Body", "Body2", "Decor", "Attached"}
for name in before:
    assert len(before[name]) == len(after[name]) == 3
    for original, restored in zip(before[name], after[name]):
        assert all(math.isclose(a, b, abs_tol=1e-6) for a, b in zip(original, restored)), (name, original, restored)
assert before["Body"][0] == (1.0, 0.0, 0.0)
assert all(math.isclose(a, b, abs_tol=1e-6) for a, b in zip(before["Body2"][0], (0.0, 3.0, 0.0)))
assert before["Decor"][0] == (5.0, 0.0, 0.0)
assert all(math.isclose(a, b, abs_tol=1e-6) for a, b in zip(before["Attached"][0], (-2.0, 1.0, 0.0)))
print("Independent multi-skin and rigid mesh deformation comparison passed")
