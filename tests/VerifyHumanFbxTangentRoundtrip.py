"""Compare human FBX tangent arrays before and after SDK import without its importer."""
import pathlib
import sys
import json
import math
import struct
from collections import defaultdict
from itertools import product
from FbxBinaryReader import child, read_fbx


def mesh(path):
    root = read_fbx(pathlib.Path(path).read_bytes())
    objects = next(node for node in root if node[0] == "Objects")[2]
    connections = next(node for node in root if node[0] == "Connections")[2]
    geometry = next(node for node in objects if node[0] == "Geometry" and node[1][2] == "Mesh")
    assert sum(node[0] == "Model" and node[1][2] == "LimbNode" for node in objects) == 163
    assert any(node[0] == "Deformer" and node[1][2] == "Skin" for node in objects)
    assert any(node[0] == "Deformer" and node[1][2] == "BlendShapeChannel" for node in objects)
    return geometry, objects, connections


(a, source_objects, source_connections), (b, _, _) = map(mesh, sys.argv[1:])
assert child(a, "PolygonVertexIndex")[1][0] == child(b, "PolygonVertexIndex")[1][0]
for layer, field in (("LayerElementTangent", "Tangents"),
                     ("LayerElementTangent", "TangentsW"),
                     ("LayerElementBinormal", "Binormals")):
    assert child(child(a, layer), field)[1][0] == child(child(b, layer), field)[1][0]
assert len(child(child(b, "LayerElementTangent"), "TangentsW")[1][0]) == 53512
print("Independent human FBX preserved all 53512 tangent/binormal corners and rig after reimport")

# Match each triangulated GLB corner to a source FBX polygon corner using its
# point, normal and UV, then compare the retained tangent and handedness.
raw = pathlib.Path(sys.argv[1] + ".glb").read_bytes()
assert struct.unpack_from("<III", raw) == (0x46546C67, 2, len(raw))
size, kind = struct.unpack_from("<II", raw, 12)
assert kind == 0x4E4F534A
doc = json.loads(raw[20:20 + size])
binary_size, kind = struct.unpack_from("<II", raw, 20 + size)
assert kind == 0x004E4942 and 28 + size + binary_size == len(raw)
binary = raw[28 + size:]


def accessor(index):
    item = doc["accessors"][index]
    size = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[item["type"]]
    code = {5123: "H", 5125: "I", 5126: "f"}[item["componentType"]]
    width = struct.calcsize("<" + code)
    view = doc["bufferViews"][item["bufferView"]]
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    stride = view.get("byteStride", size * width)
    return [struct.unpack_from("<" + code * size, binary, start + i * stride)
            for i in range(item["count"])]


def group(values, width):
    return [values[i:i + width] for i in range(0, len(values), width)]


points = group(child(a, "Vertices")[1][0], 3)
corners = child(a, "PolygonVertexIndex")[1][0]
normal_layer = child(a, "LayerElementNormal")
normals = group(child(normal_layer, "Normals")[1][0], 3)
normal_indices = child(normal_layer, "NormalsIndex")[1][0]
uv_layer = child(a, "LayerElementUV")
uvs = group(child(uv_layer, "UV")[1][0], 2)
uv_indices = child(uv_layer, "UVIndex")[1][0]
tangents = group(child(child(a, "LayerElementTangent"), "Tangents")[1][0], 3)
signs = child(child(a, "LayerElementTangent"), "TangentsW")[1][0]
bone_names = {node[1][0]: node[1][1].split("\0", 1)[0] for node in source_objects
              if node[0] == "Model" and node[1][2] == "LimbNode"}
clusters = {node[1][0]: node for node in source_objects
            if node[0] == "Deformer" and node[1][2] == "Cluster"}
cluster_bones = {edge[1][2]: bone_names[edge[1][1]] for edge in source_connections
                 if edge[0] == "C" and len(edge[1]) >= 3 and edge[1][1] in bone_names and edge[1][2] in clusters}
assert len(cluster_bones) == len(clusters) and 0 < len(clusters) <= 163
point_weights = defaultdict(dict)
for cluster_id, cluster in clusters.items():
    indexes = child(cluster, "Indexes")[1][0]
    weights = child(cluster, "Weights")[1][0]
    assert len(indexes) == len(weights)
    for point, weight in zip(indexes, weights):
        if weight:
            point_weights[point][cluster_bones[cluster_id]] = weight


def key(p):
    return tuple(round(x * 10000) for x in p)


source = defaultdict(list)
for i, corner in enumerate(corners):
    point_id = corner if corner >= 0 else -corner - 1
    point = points[point_id]
    normal = normals[normal_indices[i]]
    length = math.hypot(*normal)
    assert length > 0
    normal = tuple(n / length for n in normal)
    source[key(point)].append((point, normal, uvs[uv_indices[i]], tangents[i], signs[i], point_id))

assert len(doc["skins"]) == 1 and len(doc["skins"][0]["joints"]) == 163
primitive = doc["meshes"][0]["primitives"][0]
assert "source_fbx_morph_tangents_unavailable" in primitive["extras"]["iiActorObjectFbxTangentNotes"]
assert {"POSITION", "NORMAL"} <= set(primitive["targets"][0]) and "TANGENT" not in primitive["targets"][0]
attrs = primitive["attributes"]
positions = accessor(attrs["POSITION"])
ids = accessor(attrs["_IIACTOROBJECT_POINT_ID"])
assert len(ids) == len(positions)
identity = doc["meshes"][0]["extras"]["iiActorObjectPoints"]
view = doc["bufferViews"][identity["bufferView"]]
offset = view.get("byteOffset", 0)
assert binary[offset:offset + 8] == b"iiGLPT\0\0"
version, point_count = struct.unpack_from("<II", binary, offset + 8)
assert version == 1 and point_count == len(points)
out_normals = accessor(attrs["NORMAL"])
out_uvs = accessor(attrs["TEXCOORD_0"])
out_tangents = accessor(attrs["TANGENT"])
indices = [value[0] for value in accessor(primitive["indices"])]
assert len(indices) == len(positions) == 80268
weights = [accessor(attrs[name]) for name in sorted(attrs) if name.startswith("WEIGHTS_")]
joints = [accessor(attrs[name]) for name in sorted(attrs) if name.startswith("JOINTS_")]
assert weights and len(weights) == len(joints)
joint_names = [doc["nodes"][index]["name"] for index in doc["skins"][0]["joints"]]
offsets = tuple(product((-1, 0, 1), repeat=3))
for index in indices:
    p, n, uv, tangent = positions[index], out_normals[index], out_uvs[index], out_tangents[index]
    source_id = ids[index][0] | (ids[index][1] << 16)
    assert source_id < len(points)
    actual_weights = {}
    for js, ws in zip(joints, weights):
        for joint, weight in zip(js[index], ws[index]):
            if weight:
                assert joint < len(joint_names)
                actual_weights[joint_names[joint]] = actual_weights.get(joint_names[joint], 0) + weight
    assert abs(sum(actual_weights.values()) - 1) < 2e-5
    k = key(p)
    found = False
    for offset in offsets:
        for sp, sn, suv, st, sw, point_id in source.get(tuple(k[j] + offset[j] for j in range(3)), ()):
            expected_weights = point_weights[point_id]
            if (point_id == source_id and all(abs(p[j]-sp[j]) < 2e-5 and abs(n[j]-sn[j]) < 2e-5 and
                    abs(tangent[j]-st[j]) < 2e-5 for j in range(3)) and
                abs(uv[0]-suv[0]) < 2e-5 and abs((1-uv[1])-suv[1]) < 2e-5 and tangent[3] == -sw and
                actual_weights.keys() == expected_weights.keys() and
                all(abs(actual_weights[name]-weight) < 2e-5 for name, weight in expected_weights.items())):
                found = True
                break
        if found:
            break
    assert found, (index, p, n, uv, tangent)
print("Independent human FBX to GLB retained all 80268 corner point IDs, tangents and source skin weights across 163 joints")
