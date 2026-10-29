"""Read FBX channel/shape records independently of the SDK and desktop apps."""
import subprocess
import sys
from FbxBinaryReader import child, read_fbx

raw = bytes.fromhex(subprocess.run([sys.argv[1], "emit_grouped_morph_fbx"], check=True,
                                   text=True, stdout=subprocess.PIPE).stdout.strip())
root = read_fbx(raw)
objects = next(node for node in root if node[0] == "Objects")[2]
links = [node[1] for node in next(node for node in root if node[0] == "Connections")[2] if node[0] == "C"]
channels = {node[1][1].split("\x00", 1)[0]: node for node in objects
            if node[0] == "Deformer" and node[1][2] == "BlendShapeChannel"}
shapes = {node[1][0]: node for node in objects if node[0] == "Geometry" and node[1][2] == "Shape"}
assert set(channels) == {"Smile", "Blink"}
smile = channels["Smile"]
assert child(smile, "iiActorObjectMergedMorphRecords")[1] == [2]
attached = [link[1] for link in links if len(link) == 3 and link[0] == "OO" and link[2] == smile[1][0] and link[1] in shapes]
assert len(attached) == 1
shape = shapes[attached[0]]
assert child(shape, "Indexes")[1][0] == (2,)
xyz = child(shape, "Vertices")[1][0]
assert len(xyz) == 3 and abs(xyz[2] - .15) < 1e-12
print("Independent FBX: one logical Smile channel with both source position deltas")
