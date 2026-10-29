"""Independently inspect and evaluate generated FBX animation without editor applications."""
import subprocess
import sys
from FbxBinaryReader import child, read_fbx

raw = bytes.fromhex(subprocess.run([sys.argv[1], "fixture"], check=True, capture_output=True, text=True).stdout)
root = read_fbx(raw)
objects = next(node[2] for node in root if node[0] == "Objects")
connections = next(node[2] for node in root if node[0] == "Connections")
by_id = {node[1][0]: node for node in objects}
by_type = lambda name: [node for node in objects if node[0] == name]
assert len(by_type("AnimationStack")) == len(by_type("AnimationLayer")) == 1
assert len(by_type("AnimationCurveNode")) == 11
assert len(by_type("AnimationCurve")) == 29
assert child(next(node for node in root if node[0] == "Takes"), "Current")[1] == ["Take"]
links = [node[1] for node in connections if node[0] == "C"]
stack = by_type("AnimationStack")[0][1][0]
layer = by_type("AnimationLayer")[0][1][0]
assert ["OO", layer, stack] in links
properties = [c for c in links if c[0] == "OP" and by_id.get(c[1], (None,))[0] == "AnimationCurveNode"]
assert sum(c[3] == "DeformPercent" and by_id[c[2]][1][2] == "BlendShapeChannel" for c in properties) == 2
for prop in ("Lcl Translation", "Lcl Rotation", "Lcl Scaling"):
    assert sum(c[3] == prop for c in properties) == 3
for node in by_type("AnimationCurve"):
    times = child(node, "KeyTime")[1][0]
    values = child(node, "KeyValueFloat")[1][0]
    assert len(times) == len(values) >= 2
    assert times[0] == 0 and times[-1] == 92372316000
    assert all(a < b for a, b in zip(times, times[1:]))
    assert child(node, "KeyAttrFlags")[1][0] == (4,)
    assert child(node, "KeyAttrRefCount")[1][0] == (len(times),)
    assert child(node, "KeyAttrDataFloat")[1][0] == (0,0,0,0)
    c = next(c for c in links if c[0] == "OP" and c[1] == node[1][0])
    if c[3] == "d|DeformPercent":
        assert values == (25.,75.)
    else:
        assert c[3] in ("d|X", "d|Y", "d|Z")
        assert max(b-a for a,b in zip(times,times[1:])) <= 4618615800
subprocess.run([sys.argv[2]], input=raw.hex(), text=True, check=True)
negative = bytes.fromhex(subprocess.run([sys.argv[1], "negative_fixture"], check=True, capture_output=True, text=True).stdout)
subprocess.run([sys.argv[2], "negative"], input=negative.hex(), text=True, check=True)
scalar = bytes.fromhex(subprocess.run([sys.argv[1], "scalar_fixture"], check=True, capture_output=True, text=True).stdout)
scalar_root = read_fbx(scalar)
scalar_objects = next(node[2] for node in scalar_root if node[0] == "Objects")
scalar_curves = [node for node in scalar_objects if node[0] == "AnimationCurve"]
assert len(scalar_curves) == 2
for node in scalar_curves:
    assert child(node, "KeyTime")[1][0] == (0,11546539500,80825776500,92372316000)
    assert child(node, "KeyValueFloat")[1][0] == (-50.,-50.,150.,150.)
subprocess.run([sys.argv[2], "scalar"], input=scalar.hex(), text=True, check=True)
indexed = bytes.fromhex(subprocess.run([sys.argv[1], "indexed_fixture"], check=True, capture_output=True, text=True).stdout)
subprocess.run([sys.argv[2], "indexed"], input=indexed.hex(), text=True, check=True)
for mode in ("indexed_logical", "indexed_logical_external"):
    data = bytes.fromhex(subprocess.run([sys.argv[1], mode + "_fixture"], check=True, capture_output=True, text=True).stdout)
    subprocess.run([sys.argv[2], mode], input=data.hex(), text=True, check=True)
print("Independent FBX binary animation graph, time and curve checks passed")
