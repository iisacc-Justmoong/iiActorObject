"""Inspect multi-take FBX timelines and connections independently of the SDK."""
import subprocess
import sys

from FbxBinaryReader import child, read_fbx

raw = bytes.fromhex(subprocess.run([sys.argv[1], "emit_takes", sys.argv[2]], check=True,
                                   text=True, stdout=subprocess.PIPE).stdout.strip())
root = read_fbx(raw)
objects = next(node for node in root if node[0] == "Objects")[2]
links = [node[1] for node in next(node for node in root if node[0] == "Connections")[2] if node[0] == "C"]
stacks = {node[1][1].split("\x00", 1)[0]: node for node in objects if node[0] == "AnimationStack"}
layers = {node[1][0] for node in objects if node[0] == "AnimationLayer"}
nodes = {node[1][0] for node in objects if node[0] == "AnimationCurveNode"}
curves = {node[1][0]: node for node in objects if node[0] == "AnimationCurve"}
channels = {node[1][1].split("\x00", 1)[0]: node for node in objects
            if node[0] == "Deformer" and node[1][2] == "BlendShapeChannel"}
assert set(stacks) == {"ExpressionTake", "SecondTake"} and "Expression" in channels
tick = 46186158000
expected = {"ExpressionTake": (-tick, tick, -75., 100.),
            "SecondTake": (2 * tick, 3 * tick, 10., 80.)}
takes_node = next(node for node in root if node[0] == "Takes")
assert child(takes_node, "Current")[1] == ["ExpressionTake"]
takes = {node[1][0]: node for node in takes_node[2] if node[0] == "Take"}
assert set(takes) == set(expected)
for name, (start, end, first, last) in expected.items():
    properties = child(stacks[name], "Properties70")[2]
    props = {node[1][0]: node[1][-1] for node in properties if node[0] == "P"}
    assert props["LocalStart"] == props["ReferenceStart"] == start
    assert props["LocalStop"] == props["ReferenceStop"] == end
    assert child(takes[name], "LocalTime")[1] == [start, end]
    assert child(takes[name], "ReferenceTime")[1] == [start, end]
    filename = child(takes[name], "FileName")[1][0]
    assert filename.endswith(".tak") and "/" not in filename and "\\" not in filename
    stack_layers = {link[1] for link in links if len(link) == 3 and link[0] == "OO"
                    and link[2] == stacks[name][1][0] and link[1] in layers}
    assert len(stack_layers) == 1
    layer_nodes = {link[1] for link in links if len(link) == 3 and link[0] == "OO"
                   and link[2] in stack_layers and link[1] in nodes}
    expression_nodes = {link[1] for link in links if len(link) == 4 and link[0] == "OP"
                        and link[2:] == [channels["Expression"][1][0], "DeformPercent"]
                        and link[1] in layer_nodes}
    assert len(expression_nodes) == 1
    curve_ids = [link[1] for link in links if len(link) == 4 and link[0] == "OP"
                 and link[2] in expression_nodes and link[3] == "d|DeformPercent" and link[1] in curves]
    assert len(curve_ids) == 1
    curve = curves[curve_ids[0]]
    times = child(curve, "KeyTime")[1][0]
    values = child(curve, "KeyValueFloat")[1][0]
    assert times[0] == start and times[-1] == end and all(a < b for a, b in zip(times, times[1:]))
    assert abs(values[0] - first) < 1e-4 and abs(values[-1] - last) < 1e-4
print("Independent FBX take names, signed timeline origins and progressive curves passed")
