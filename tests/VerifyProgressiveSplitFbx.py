"""Independently inspect FBX geometry, channel groups and duplicate animation curves."""
import subprocess
import sys

from FbxBinaryReader import child, read_fbx


def read(mode):
    result = subprocess.run([sys.argv[1], mode], check=True, text=True, stdout=subprocess.PIPE)
    root = read_fbx(bytes.fromhex(result.stdout.strip()))
    objects = next(node for node in root if node[0] == "Objects")[2]
    links = [node[1] for node in next(node for node in root if node[0] == "Connections")[2]
             if node[0] == "C"]
    return objects, links


def name(node):
    return node[1][1].split("\x00", 1)[0]


def inspect(objects, links, animated):
    meshes = {n[1][0]: n for n in objects if n[0] == "Geometry" and n[1][2] == "Mesh"}
    models = {n[1][0]: n for n in objects if n[0] == "Model" and n[1][2] == "Mesh"}
    blends = {n[1][0]: n for n in objects if n[0] == "Deformer" and n[1][2] == "BlendShape"}
    shapes = {n[1][0]: n for n in objects if n[0] == "Geometry" and n[1][2] == "Shape"}
    channels = [n for n in objects if n[0] == "Deformer" and n[1][2] == "BlendShapeChannel"]
    assert len(meshes) == len(models) == len(blends) == 3
    assert len(channels) == 5

    def destinations(source, available):
        return [link[2] for link in links if len(link) == 3 and link[:2] == ["OO", source]
                and link[2] in available]

    per_control = {}
    near = {}
    for channel in channels:
        channel_name = name(channel)
        if channel_name == "Other":
            continue
        assert channel_name in ("Expression", "Blink")
        assert child(channel, "DeformPercent")[1] == ([15.] if channel_name == "Expression" else [10.])
        group = child(channel, "iiActorObjectProgressiveGroup")[1][0]
        assert child(channel, "iiActorObjectProgressiveGroupSize")[1] == [2]
        blend_ids = destinations(channel[1][0], blends)
        assert len(blend_ids) == 1
        geometry_ids = destinations(blend_ids[0], meshes)
        assert len(geometry_ids) == 1
        model_ids = destinations(geometry_ids[0], models)
        assert len(model_ids) == 1
        mesh_name = name(models[model_ids[0]])
        per_control.setdefault(channel_name, []).append((group, channel[1][0], mesh_name))
        if channel_name == "Expression":
            attached = [shapes[link[1]] for link in links if len(link) == 3 and link[0] == "OO"
                        and link[2] == channel[1][0] and link[1] in shapes]
            target = next(shape for shape in attached if name(shape) == "PositiveNear")
            near[mesh_name] = child(target, "Vertices")[1][0][1]
    assert {k: {item[2] for item in v} for k, v in per_control.items()} == {
        "Expression": {"Face", "Face2"}, "Blink": {"Face", "Face3"}}
    assert all(len({item[0] for item in v}) == 1 for v in per_control.values())
    assert per_control["Expression"][0][0] != per_control["Blink"][0][0]
    assert abs(near["Face"] - .25) < 1e-12 and abs(near["Face2"] - .9) < 1e-12

    if not animated:
        return
    nodes = {n[1][0]: n for n in objects if n[0] == "AnimationCurveNode"}
    curves = {n[1][0]: n for n in objects if n[0] == "AnimationCurve"}
    for control, entries in per_control.items():
        samples = []
        for _, channel_id, _ in entries:
            node_ids = [link[1] for link in links if len(link) == 4 and link[0] == "OP"
                        and link[2:] == [channel_id, "DeformPercent"] and link[1] in nodes]
            assert len(node_ids) == 1
            curve_ids = [link[1] for link in links if len(link) == 4 and link[0] == "OP"
                         and link[2:] == [node_ids[0], "d|DeformPercent"] and link[1] in curves]
            assert len(curve_ids) == 1
            curve = curves[curve_ids[0]]
            samples.append((child(curve, "KeyTime")[1][0], child(curve, "KeyValueFloat")[1][0]))
        assert samples[0] == samples[1]
        assert samples[0][0] == (0, 46186158000, 92372316000)
        assert samples[0][1] == ((0., 55., 100.) if control == "Expression" else (0., 20., 50.))


inspect(*read("emit_split"), animated=False)
inspect(*read("emit_split_animated"), animated=True)
print("Independent FBX multi-geometry progressive channels and animation passed")
