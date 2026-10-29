"""Inspect progressive FBX channel records without relying on an application."""
import subprocess
import sys

from FbxBinaryReader import child, read_fbx

raw = bytes.fromhex(subprocess.run([sys.argv[1], "emit_export", sys.argv[2]],
                                   check=True, text=True, stdout=subprocess.PIPE).stdout.strip())
root = read_fbx(raw)
objects = next(node for node in root if node[0] == "Objects")[2]
links = [node[1] for node in next(node for node in root if node[0] == "Connections")[2] if node[0] == "C"]
channels = [node for node in objects if node[0] == "Deformer" and node[1][2] == "BlendShapeChannel"]
shapes = {node[1][0]: node for node in objects if node[0] == "Geometry" and node[1][2] == "Shape"}
assert len(channels) == 1
channel = channels[0]
assert channel[1][1].split("\x00", 1)[0] == "Expression"
assert child(channel, "DeformPercent")[1] == [15.0]
assert child(channel, "FullWeights")[1][0] == (-75., -20., 30., 80., 100.)
attached = [shapes[link[1]] for link in links if len(link) == 3 and link[0] == "OO"
            and link[2] == channel[1][0] and link[1] in shapes]
assert len(attached) == 5
named = {shape[1][1].split("\x00", 1)[0]: shape for shape in attached}
assert set(named) == {"NegativeFar", "NegativeNear", "PositiveNear", "PositiveMid", "PositiveFar"}
near = named["PositiveNear"]
assert child(near, "Indexes")[1][0] == (0,)
assert abs(child(near, "Vertices")[1][0][1] - .5) < 1e-12
assert len(child(near, "Normals")[1][0]) == 3

shared = bytes.fromhex(subprocess.run([sys.argv[1], "emit_shared", sys.argv[2]],
                                      check=True, text=True, stdout=subprocess.PIPE).stdout.strip())
root = read_fbx(shared)
objects = next(node for node in root if node[0] == "Objects")[2]
links = [node[1] for node in next(node for node in root if node[0] == "Connections")[2] if node[0] == "C"]
meshes = [node for node in objects if node[0] == "Geometry" and node[1][2] == "Mesh"]
models = {node[1][0] for node in objects if node[0] == "Model" and node[1][2] == "Mesh"}
channels = [node for node in objects if node[0] == "Deformer" and node[1][2] == "BlendShapeChannel"]
assert len(meshes) == 1 and len(models) == 2 and len(channels) == 1
mesh_id = meshes[0][1][0]
assert {link[2] for link in links if len(link) == 3 and link[:2] == ["OO", mesh_id]} == models
print("Independent FBX progressive shapes, defaults, normals and shared geometry passed")
