"""Independently verify exported skin assignments for two same-facing surfaces."""
from collections import defaultdict
from pathlib import Path
import subprocess
import sys
import tempfile

from FbxBinaryReader import child, read_fbx
from VerifyCharacterGlb import accessor, read_glb

exe, build = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="adapted-identity-", dir=build) as temporary:
    directory = Path(temporary)
    subprocess.run([exe, "identity_formats", str(directory)], check=True)
    subprocess.run([exe, "permuted_formats", str(directory)], check=True)
    subprocess.run([exe, "shuffled_formats", str(directory)], check=True)
    subprocess.run([exe, "collision_formats", str(directory)], check=True)

    def verify(name, permuted):
        root = read_fbx((directory / f"{name}.fbx").read_bytes())
        objects = next(node for node in root if node[0] == "Objects")[2]
        links = {tuple(node[1][1:3]) for node in next(node for node in root if node[0] == "Connections")[2]
                 if node[0] == "C" and node[1][0] == "OO" and len(node[1]) == 3}
        bones = {node[1][0]: node[1][1].split("\0", 1)[0] for node in objects
                 if node[0] == "Model" and node[1][2] == "LimbNode"}
        clusters = {node[1][0]: node for node in objects if node[0] == "Deformer" and node[1][2] == "Cluster"}
        assert set(bones.values()) == {"outer", "inner"}
        rows = defaultdict(dict)
        for identity, cluster in clusters.items():
            names = [bone_name for bone, bone_name in bones.items() if (bone, identity) in links]
            assert len(names) == 1
            indices, weights = child(cluster, "Indexes")[1][0], child(cluster, "Weights")[1][0]
            assert len(indices) == len(weights)
            for point, weight in zip(indices, weights):
                rows[point][names[0]] = weight
        for point in range(6):
            outer = (point >= 3) if permuted else (point < 3)
            assert rows[point] == {"outer" if outer else "inner": 1.0}

        document, binary = read_glb(directory / f"{name}.glb")
        skin = document["skins"][0]
        names = [document["nodes"][node]["name"] for node in skin["joints"]]
        primitive = document["meshes"][0]["primitives"][0]
        attrs = primitive["attributes"]
        positions = accessor(document, binary, attrs["POSITION"])
        joints = accessor(document, binary, attrs["JOINTS_0"])
        weights = accessor(document, binary, attrs["WEIGHTS_0"])
        assert len(positions) == len(joints) == len(weights) == 6
        for position, row, amounts in zip(positions, joints, weights):
            actual = {names[joint]: amount for joint, amount in zip(row, amounts) if amount}
            expected = ("outer" if position[2] < .005 else "inner") if name in {"shuffled", "collision"} else (
                "outer" if position[2] > .0000005 else "inner")
            assert actual == {expected: 1.0}
        if name in {"shuffled", "collision"}:
            assert document["meshes"][0]["extras"]["targetNames"] == ["tip"]
            deltas = accessor(document, binary, primitive["targets"][0]["POSITION"])
            assert len(deltas) == len(positions)
            for position, delta in zip(positions, deltas):
                tip = position[2] < .005 and abs(position[0]) < .0001 and abs(position[1]) < .0001
                assert abs(delta[0] - (.02 if tip else 0)) < 1e-7
                assert abs(delta[1]) < 1e-7 and abs(delta[2]) < 1e-7

    verify("identity", False)
    verify("permuted", True)
    verify("shuffled", True)
    verify("collision", True)
print("Independent FBX and GLB retain skin and morph data across point and face reordering")
