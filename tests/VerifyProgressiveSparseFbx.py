"""Independently inspect sparse FBX in-betweens and per-mesh animation."""
import struct
import subprocess
import sys

from FbxBinaryReader import child, read_fbx


def load(mode):
    out = subprocess.run([sys.argv[1], mode], check=True, text=True, stdout=subprocess.PIPE).stdout
    root = read_fbx(bytes.fromhex(out.strip()))
    objects = next(node for node in root if node[0] == "Objects")[2]
    links = [node[1] for node in next(node for node in root if node[0] == "Connections")[2]
             if node[0] == "C"]
    return objects, links


def name(node):
    return node[1][1].split("\x00", 1)[0]


def verify(objects, links, animated):
    meshes = {n[1][0]: n for n in objects if n[0] == "Geometry" and n[1][2] == "Mesh"}
    models = {n[1][0]: n for n in objects if n[0] == "Model" and n[1][2] == "Mesh"}
    blends = {n[1][0]: n for n in objects if n[0] == "Deformer" and n[1][2] == "BlendShape"}
    shapes = {n[1][0]: n for n in objects if n[0] == "Geometry" and n[1][2] == "Shape"}
    channels = [n for n in objects if n[0] == "Deformer" and n[1][2] == "BlendShapeChannel"]
    assert len(meshes) == len(models) == len(blends) == 3
    assert [sum(name(n) == label for n in channels) for label in ("Sparse", "Overlay", "Other")] == [3, 2, 1]

    def destinations(source, available):
        return [link[2] for link in links if len(link) == 3 and link[:2] == ["OO", source]
                and link[2] in available]

    expected = {"Face": (0, 1, 1), "Face2": (1, 1, 0), "Face3": (0, 0, 1)}
    channel_ids = {"Sparse": [], "Overlay": []}
    group_ids = {"Sparse": set(), "Overlay": set()}
    for channel in channels:
        label = name(channel)
        if label == "Other":
            assert child(channel, "DeformPercent")[1] == [10.]
            continue
        assert label in channel_ids
        channel_ids[label].append(channel[1][0])
        group_ids[label].add(child(channel, "iiActorObjectProgressiveGroup")[1][0])
        assert child(channel, "iiActorObjectProgressiveGroupSize")[1] == ([3] if label == "Sparse" else [2])
        blend, = destinations(channel[1][0], blends)
        mesh, = destinations(blend, meshes)
        model, = destinations(mesh, models)
        mesh_name = name(models[model])
        if label == "Overlay":
            assert mesh_name in ("Face", "Face2")
            assert not any(n[0] == "iiActorObjectProgressivePresence" for n in channel[2])
            continue
        assert child(channel, "iiActorObjectProgressiveSparse")[1] == [1]
        raw_presence, = child(channel, "iiActorObjectProgressivePresence")[1]
        presence = struct.unpack("<III", raw_presence)
        assert presence == expected[mesh_name]
        assert child(channel, "FullWeights")[1][0] == (-100., 50., 100.)
        attached = [shapes[link[1]] for link in links if len(link) == 3 and link[0] == "OO"
                    and link[2] == channel[1][0] and link[1] in shapes]
        by_name = {name(shape): shape for shape in attached}
        assert set(by_name) == {"Negative", "Mid", "Full"}
        for present, key in zip(presence, ("Negative", "Mid", "Full")):
            indices = child(by_name[key], "Indexes")[1][0]
            values = child(by_name[key], "Vertices")[1][0]
            assert (len(indices), len(values)) == ((1, 3) if present else (0, 0))
    assert len(group_ids["Sparse"]) == len(group_ids["Overlay"]) == 1
    assert group_ids["Sparse"] != group_ids["Overlay"]

    if not animated:
        return
    nodes = {n[1][0]: n for n in objects if n[0] == "AnimationCurveNode"}
    curves = {n[1][0]: n for n in objects if n[0] == "AnimationCurve"}
    for label, targets in channel_ids.items():
        samples = []
        for target in targets:
            node, = [link[1] for link in links if len(link) == 4 and link[0] == "OP"
                     and link[2:] == [target, "DeformPercent"] and link[1] in nodes]
            curve, = [link[1] for link in links if len(link) == 4 and link[0] == "OP"
                      and link[2:] == [node, "d|DeformPercent"] and link[1] in curves]
            samples.append((child(curves[curve], "KeyTime")[1][0],
                            child(curves[curve], "KeyValueFloat")[1][0]))
        assert all(sample == samples[0] for sample in samples)
        assert samples[0][0] == (0, 46186158000, 92372316000)
        assert samples[0][1] == ((-100., 75., 100.) if label == "Sparse" else (0., 15., 30.))


verify(*load("emit_sparse"), animated=False)
verify(*load("emit_sparse_animated"), animated=True)
print("Independent FBX sparse target membership, neutral shapes and animation passed")
