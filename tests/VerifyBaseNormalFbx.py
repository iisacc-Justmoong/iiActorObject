"""Inspect authored normal values and corner indices without the SDK FBX reader."""
import subprocess
import sys
from FbxBinaryReader import child, read_fbx

raw = bytes.fromhex(subprocess.run([sys.argv[1], "emit_base_normal_fbx"], check=True,
                                   text=True, stdout=subprocess.PIPE).stdout.strip())
objects = next(node for node in read_fbx(raw) if node[0] == "Objects")[2]
mesh = next(node for node in objects if node[0] == "Geometry" and node[1][2] == "Mesh")
layer = child(mesh, "LayerElementNormal")
assert child(layer, "MappingInformationType")[1] == ["ByPolygonVertex"]
assert child(layer, "ReferenceInformationType")[1] == ["IndexToDirect"]
values = child(layer, "Normals")[1][0]
indices = child(layer, "NormalsIndex")[1][0]
assert len(indices) == 4
for index in indices:
    normal = values[index * 3:index * 3 + 3]
    assert len(normal) == 3 and all(abs(a - b) < 1e-7 for a, b in zip(normal, (.6, 0, .8)))
print("Independent FBX: four source corners reference the edited normal")
