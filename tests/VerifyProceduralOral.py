"""Read generated oral mesh and blend-shape data without the SDK or desktop apps."""

from pathlib import Path
import sys

from FbxBinaryReader import child, read_fbx
from VerifyCharacterGlb import accessor, read_glb


def main(directory):
    values = iter((directory / "tongue.txt").read_text().split())
    count = int(next(values))
    expected = [[float(next(values)) for _ in range(6)] for _ in range(count)]
    assert next(values, None) is None
    document, binary = read_glb(directory / "human.glb")
    mesh = next(mesh for mesh in document["meshes"] if mesh["name"] == "Tongue")
    names = mesh["extras"]["targetNames"]
    assert {"tongueOut", "jawOpen", "jawForward", "jawLeft", "jawRight"} <= set(names)
    primitive = mesh["primitives"][0]
    positions = accessor(document, binary, primitive["attributes"]["POSITION"])
    deltas = accessor(document, binary, primitive["targets"][names.index("tongueOut")]["POSITION"])
    normals = accessor(document, binary, primitive["targets"][names.index("tongueOut")]["NORMAL"])
    joints = accessor(document, binary, primitive["attributes"]["JOINTS_0"])
    weights = accessor(document, binary, primitive["attributes"]["WEIGHTS_0"])
    jaw = next(i for i, node in enumerate(document["nodes"]) if node.get("name") == "jaw")
    skin = document["skins"][0]
    jaw_index = skin["joints"].index(jaw)
    assert len(positions) == len(deltas) == len(normals) == len(joints) == len(weights) == count
    assert max(row[2] for row in deltas) > .025
    for i in range(count):
        assert joints[i][0] == jaw_index and abs(weights[i][0] - 1) < 1e-7
        assert all(abs(a - b) < 1e-6 for a, b in zip(positions[i] + deltas[i], expected[i]))

    tree = read_fbx((directory / "human.fbx").read_bytes())
    objects = next(node for node in tree if node[0] == "Objects")[2]
    links = [node[1] for node in next(node for node in tree if node[0] == "Connections")[2] if node[0] == "C"]
    mesh_node = next(node for node in objects if node[0] == "Geometry" and node[1][2] == "Mesh"
                     and node[1][1].split("\x00", 1)[0] == "Tongue")
    mesh_id = mesh_node[1][0]
    positions_fbx = child(mesh_node, "Vertices")[1][0]
    assert len(positions_fbx) == 3 * count
    for i in range(count):
        assert all(abs(positions_fbx[3 * i + c] - expected[i][c]) < 1e-9 for c in range(3))
    jaw_id = next(node[1][0] for node in objects if node[0] == "Model" and node[1][1].split("\x00", 1)[0] == "jaw")
    skins = {node[1][0] for node in objects if node[0] == "Deformer" and node[1][2] == "Skin"}
    clusters = {node[1][0]: node for node in objects if node[0] == "Deformer" and node[1][2] == "Cluster"}
    tongue_skins = [link[1] for link in links if len(link) == 3 and link[0] == "OO" and link[1] in skins and link[2] == mesh_id]
    assert len(tongue_skins) == 1
    tongue_clusters = [link[1] for link in links if len(link) == 3 and link[0] == "OO" and link[1] in clusters and link[2] == tongue_skins[0]]
    assert len(tongue_clusters) == 1 and ["OO", jaw_id, tongue_clusters[0]] in links
    indices = child(clusters[tongue_clusters[0]], "Indexes")[1][0]
    weights_fbx = child(clusters[tongue_clusters[0]], "Weights")[1][0]
    assert list(indices) == list(range(count)) and all(abs(w - 1) < 1e-12 for w in weights_fbx)
    channels = [node for node in objects if node[0] == "Deformer" and node[1][2] == "BlendShapeChannel"
                and node[1][1].split("\x00", 1)[0] == "tongueOut"]
    shapes = {node[1][0]: node for node in objects if node[0] == "Geometry" and node[1][2] == "Shape"}
    matching = []
    for channel in channels:
        for link in links:
            if len(link) == 3 and link[0] == "OO" and link[2] == channel[1][0] and link[1] in shapes:
                shape = shapes[link[1]]
                indices = child(shape, "Indexes")[1][0]
                deltas_fbx = child(shape, "Vertices")[1][0]
                if indices and any(abs(v) > 1e-9 for v in deltas_fbx):
                    matching.append((indices, deltas_fbx))
    assert len(matching) == 1
    indices, deltas_fbx = matching[0]
    assert len(deltas_fbx) == 3 * len(indices)
    for i, index in enumerate(indices):
        assert 0 <= index < count
        assert all(abs(deltas_fbx[3 * i + c] - expected[index][3 + c]) < 1e-9 for c in range(3))
    animated, animation_binary = read_glb(directory / "human-animated.glb")
    animation = animated["animations"][0]
    seen = set()
    for channel in animation["channels"]:
        target = channel["target"]
        assert target["path"] == "weights"
        node = animated["nodes"][target["node"]]
        mesh = animated["meshes"][node["mesh"]]
        name = mesh["name"]
        assert name in ("Body", "Tongue") and name not in seen
        seen.add(name)
        names = mesh["extras"]["targetNames"]
        sampler = animation["samplers"][channel["sampler"]]
        times = [v[0] for v in accessor(animated, animation_binary, sampler["input"])]
        weights = [v[0] for v in accessor(animated, animation_binary, sampler["output"])]
        assert times == [0, .5, 1] and len(weights) == len(times) * len(names)
        for frame, (jaw, tongue) in enumerate(((0, .1), (.8, .5), (.2, .9))):
            assert abs(weights[frame * len(names) + names.index("jawOpen")] - jaw) < 1e-6
            assert abs(weights[frame * len(names) + names.index("tongueOut")] - tongue) < 1e-6
    assert seen == {"Body", "Tongue"}
    print("Independent GLB and FBX: tongue skin, channel names and morph positions passed")


if __name__ == "__main__":
    main(Path(sys.argv[1]))
