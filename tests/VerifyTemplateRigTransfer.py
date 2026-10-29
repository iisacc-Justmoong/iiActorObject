"""Independent FBX/GLB comparison of all joint influences before and after transfer."""
import json
import math
import pathlib
import struct
import sys
from collections import defaultdict
from FbxBinaryReader import child, read_fbx


def fbx(path):
    nodes = read_fbx(path.read_bytes())
    objects = next(n for n in nodes if n[0] == "Objects")[2]
    connections = next(n for n in nodes if n[0] == "Connections")[2]
    models = {n[1][0]: n[1][1] for n in objects if n[0] == "Model" and n[1][2] == "LimbNode"}
    clusters = {n[1][0]: n for n in objects if n[0] == "Deformer" and n[1][2] == "Cluster"}
    cluster_bone = {n[1][2]: models[n[1][1]] for n in connections if n[0] == "C" and
                    n[1][0] == "OO" and n[1][1] in models and n[1][2] in clusters}
    assert len(models) == 163 and len(cluster_bone) == len(clusters)
    mesh = next(n for n in objects if n[0] == "Geometry" and n[1][2] == "Mesh")
    visible = {v if v >= 0 else -v-1 for v in child(mesh, "PolygonVertexIndex")[1][0]}
    rows = defaultdict(dict)
    for identity, cluster in clusters.items():
        indices = child(cluster, "Indexes")[1][0]
        weights = child(cluster, "Weights")[1][0]
        assert len(indices) == len(weights)
        bone = cluster_bone[identity]
        for point, value in zip(indices, weights):
            assert point >= 0 and math.isfinite(value) and 0 < value <= 1 and bone not in rows[point]
            rows[point][bone] = value
    for point in visible:
        assert rows[point] and abs(sum(rows[point].values())-1) < 1e-9
    return visible, rows


def glb(path):
    raw = path.read_bytes()
    assert struct.unpack_from("<III", raw) == (0x46546c67, 2, len(raw))
    size, kind = struct.unpack_from("<II", raw, 12)
    assert kind == 0x4e4f534a
    doc = json.loads(raw[20:20+size]); offset = 20+size
    bin_size, kind = struct.unpack_from("<II", raw, offset)
    assert kind == 0x004e4942 and offset+8+bin_size == len(raw)
    binary = raw[offset+8:]
    joints = doc["skins"][0]["joints"]
    assert len(joints) == 163
    names = [doc["nodes"][j]["name"] for j in joints]

    def accessor(index):
        item = doc["accessors"][index]; view = doc["bufferViews"][item["bufferView"]]
        assert item["type"] == "VEC4"
        code = {5121:"B", 5123:"H", 5125:"I", 5126:"f"}[item["componentType"]]
        width = struct.calcsize("<"+code); start = view.get("byteOffset",0)+item.get("byteOffset",0)
        stride = view.get("byteStride",4*width)
        return [struct.unpack_from("<"+code*4,binary,start+i*stride) for i in range(item["count"])]

    rows = []
    for primitive in doc["meshes"][0]["primitives"]:
        attrs = primitive["attributes"]
        count = doc["accessors"][attrs["JOINTS_0"]]["count"]
        local = [dict() for _ in range(count)]
        index = 0
        while f"JOINTS_{index}" in attrs:
            assert f"WEIGHTS_{index}" in attrs
            js, ws = accessor(attrs[f"JOINTS_{index}"]), accessor(attrs[f"WEIGHTS_{index}"])
            assert len(js) == len(ws) == count
            for row, ji, wi in zip(local, js, ws):
                for j, w in zip(ji, wi):
                    assert math.isfinite(w) and 0 <= w <= 1
                    if w:
                        assert j < len(names) and names[j] not in row
                        row[names[j]] = w
            index += 1
        for row in local:
            assert row and abs(sum(row.values())-1) < 2e-5
        rows.extend(local)
    return rows


def equal_rows(a, b, tolerance):
    assert a.keys() == b.keys()
    assert all(abs(a[key]-b[key]) < tolerance for key in a)


def main():
    target_fbx, target_glb, reference_fbx, reference_glb = map(pathlib.Path, sys.argv[1:])
    visible, output = fbx(target_fbx)
    ref_visible, reference = fbx(reference_fbx)
    assert visible == ref_visible and len(visible) > 10_000
    for point in visible:
        equal_rows(output[point], reference[point], 1e-9)
    output, reference = glb(target_glb), glb(reference_glb)
    assert len(output) == len(reference) and len(output) > 10_000
    for a, b in zip(output, reference):
        equal_rows(a, b, 2e-6)
    print(f"Independent rig transfer: {len(visible)} FBX points, {len(output)} GLB vertices, all influences preserved")


if __name__ == "__main__":
    main()
