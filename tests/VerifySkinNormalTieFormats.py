"""Read edited GLB and FBX weights without using the SDK importer."""
from pathlib import Path
import sys

from FbxBinaryReader import child, read_fbx
from VerifyCharacterGlb import accessor, read_glb

directory = Path(sys.argv[1])
document, binary = read_glb(directory / "linear.glb")
skin = document["skins"][0]
names = [document["nodes"][index]["name"] for index in skin["joints"]]
assert names == ["outer", "inner"]
primitive = document["meshes"][0]["primitives"][0]
attrs = primitive["attributes"]
joints = accessor(document, binary, attrs["JOINTS_0"])
weights = accessor(document, binary, attrs["WEIGHTS_0"])
assert len(joints) == len(weights) == 4
for row, values in zip(joints, weights):
    assert abs(sum(values) - 1) < 1e-6
    assert [(names[index], round(weight, 6)) for index, weight in zip(row, values) if weight > 0] == [("outer", 1.0)]

for filename, expected in (("linear.fbx", "Linear"), ("blended.fbx", "Blend")):
    root = read_fbx((directory / filename).read_bytes())
    objects = next(node[2] for node in root if node[0] == "Objects")
    connections = next(node[2] for node in root if node[0] == "Connections")
    bones = {node[1][0]: node[1][1].split("\0", 1)[0] for node in objects
             if node[0] == "Model" and node[1][2] == "LimbNode"}
    clusters = {node[1][0]: node for node in objects if node[0] == "Deformer" and node[1][2] == "Cluster"}
    cluster_bones = {edge[1][2]: bones[edge[1][1]] for edge in connections
                     if edge[0] == "C" and len(edge[1]) >= 3 and edge[1][1] in bones and edge[1][2] in clusters}
    assert set(cluster_bones) == set(clusters)
    rows = {i: [] for i in range(4)}
    for identity, cluster in clusters.items():
        indices = child(cluster, "Indexes")[1][0]
        values = child(cluster, "Weights")[1][0]
        assert len(indices) == len(values)
        for point, value in zip(indices, values):
            if value:
                rows[point].append((cluster_bones[identity], value))
    for row in rows.values():
        assert len(row) == 1 and row[0][0] == "outer" and abs(row[0][1] - 1) < 1e-9
    deformers = [node for node in objects if node[0] == "Deformer" and node[1][2] == "Skin"]
    assert len(deformers) == 1 and child(deformers[0], "SkinningType")[1][0] == expected
    if expected == "Blend":
        assert child(deformers[0], "Indexes")[1][0] == (0, 1, 2, 3)
        values = child(deformers[0], "BlendWeights")[1][0]
        assert len(values) == 4 and all(abs(value - .1) < 1e-6 for value in values)

print("Independent GLB and FBX: edited joint weights and linear/blended skin profiles passed")
