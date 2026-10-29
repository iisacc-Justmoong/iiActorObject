"""Independently inspect FBX connections for progressive and ordinary animation."""
import subprocess
import sys

from FbxBinaryReader import child, read_fbx

raw = bytes.fromhex(subprocess.run([sys.argv[1], "emit_export", sys.argv[2]], check=True,
                                   text=True, stdout=subprocess.PIPE).stdout.strip())
root = read_fbx(raw)
objects = next(node for node in root if node[0] == "Objects")[2]
links = [node[1] for node in next(node for node in root if node[0] == "Connections")[2] if node[0] == "C"]
channels = {node[1][1].split("\x00", 1)[0]: node for node in objects
            if node[0] == "Deformer" and node[1][2] == "BlendShapeChannel"}
assert set(channels) == {"Expression", "Smile"}
assert child(channels["Expression"], "FullWeights")[1][0] == (-75., -20., 30., 80., 100.)
nodes = {node[1][0]: node for node in objects if node[0] == "AnimationCurveNode"}
curves = {node[1][0]: node for node in objects if node[0] == "AnimationCurve"}
for name, first, last in (("Expression", -75., 100.), ("Smile", 0., 100.)):
    target = channels[name][1][0]
    attached_nodes = [link[1] for link in links if len(link) == 4 and link[0] == "OP"
                      and link[2:] == [target, "DeformPercent"] and link[1] in nodes]
    assert len(attached_nodes) == 1
    attached_curves = [link[1] for link in links if len(link) == 4 and link[0] == "OP"
                       and link[2:] == [attached_nodes[0], "d|DeformPercent"] and link[1] in curves]
    assert len(attached_curves) == 1
    curve = curves[attached_curves[0]]
    times = child(curve, "KeyTime")[1][0]
    values = child(curve, "KeyValueFloat")[1][0]
    assert len(times) == len(values) >= 2 and times[0] == 0 and times[-1] == 92372316000
    assert all(a < b for a, b in zip(times, times[1:]))
    assert abs(values[0] - first) < 1e-4 and abs(values[-1] - last) < 1e-4
print("Independent FBX progressive and ordinary animation channel connections passed")
