"""Independently read GLB data and compare it to the source DQ runtime fixture."""

import math
from pathlib import Path
import sys

from VerifyCharacterGlb import accessor, read_glb


def multiply(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def local_matrix(node):
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])
    sx, sy, sz = node.get("scale", [1, 1, 1])
    tx, ty, tz = node.get("translation", [0, 0, 0])
    rotation = [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ]
    return [[rotation[r][c] * [sx, sy, sz][c] for c in range(3)] + [[tx, ty, tz][r]] for r in range(3)] + [[0, 0, 0, 1]]


def check(path):
    document, binary = read_glb(path.with_suffix(".glb"))
    assert document["asset"]["version"] == "2.0"
    mesh = document["meshes"][0]
    assert mesh["extras"]["targetNames"] == ["Lift", "Bend"]
    assert mesh["weights"] == [0, 0]
    primitive = mesh["primitives"][0]
    attributes = primitive["attributes"]
    positions = accessor(document, binary, attributes["POSITION"])
    normals = accessor(document, binary, attributes["NORMAL"])
    joints = accessor(document, binary, attributes["JOINTS_0"])
    weights = accessor(document, binary, attributes["WEIGHTS_0"])
    targets = [[accessor(document, binary, t[k]) for k in ("POSITION", "NORMAL")] for t in primitive["targets"]]
    skin = document["skins"][0]
    assert [document["nodes"][i]["name"] for i in skin["joints"]] == ["root", "tip", "link"]
    parents = {child: i for i, node in enumerate(document["nodes"]) for child in node.get("children", [])}
    def world(index):
        matrix = local_matrix(document["nodes"][index])
        return multiply(world(parents[index]), matrix) if index in parents else matrix
    inverse_binds = accessor(document, binary, skin["inverseBindMatrices"])
    assert len(skin["joints"]) == len(inverse_binds)
    skin_matrices = []
    for index, inverse in zip(skin["joints"], inverse_binds):
        matrix = multiply(world(index), [[inverse[4 * c + r] for c in range(4)] for r in range(4)])
        for r in range(4):
            for c in range(4):
                assert abs(matrix[r][c] - (1 if r == c else 0)) < 2e-6
        skin_matrices.append(matrix)
    raw = iter(path.with_suffix(".txt").read_text().split())
    vertex_count, case_count = int(next(raw)), int(next(raw))
    assert vertex_count == len(positions) == len(normals) == len(joints) == len(weights)
    assert len(accessor(document, binary, primitive["indices"])) % 3 == 0
    for _ in range(case_count):
        offsets = [float(next(raw)), float(next(raw))]
        for v in range(vertex_count):
            expected = [float(next(raw)) for _ in range(6)]
            assert abs(sum(weights[v]) - 1) < 1e-6
            assert all(0 <= j < len(skin_matrices) for j in joints[v])
            position = [positions[v][c] + sum(offsets[t] * targets[t][0][v][c] for t in range(2)) for c in range(3)] + [1]
            actual_position = [sum(weights[v][k] * sum(skin_matrices[joints[v][k]][r][c] * position[c]
                                for c in range(4)) for k in range(4)) for r in range(3)]
            normal = [normals[v][c] + sum(offsets[t] * targets[t][1][v][c] for t in range(2)) for c in range(3)]
            length = math.sqrt(sum(c * c for c in normal))
            actual = actual_position + [c / length for c in normal]
            assert len(actual) == len(expected)
            assert all(abs(a - b) < 3e-6 * max(1, abs(b)) for a, b in zip(actual, expected)), (path, v, actual, expected)
    assert next(raw, None) is None


if __name__ == "__main__":
    directory = Path(sys.argv[1])
    for stem in ("render-dq", "render-blended", "topology-dq", "topology-blended"):
        check(directory / stem)
    print("Independent GLB: bind matrices, weights, positions and morph normals match the source DQ runtime")
