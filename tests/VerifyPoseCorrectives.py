"""Decode corrective samples and their project envelope without the SDK reader."""
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib
from FbxBinaryReader import child, read_fbx
from VerifyCharacterGlb import accessor, read_glb

exe, build = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="pose-corrective-", dir=build) as directory:
    subprocess.run([exe, "formats", directory], check=True)
    root = Path(directory)
    program = (root / "corrective.iipc").read_bytes()
    assert program[:8] == b"iiAOPCR\0"
    version, length, checksum = struct.unpack_from("<IQI", program, 8)
    payload = program[24:]
    assert version == 1 and length == len(payload) and checksum == zlib.crc32(payload)
    cursor = 0
    def integer():
        global cursor
        result = struct.unpack_from("<I", payload, cursor)[0]
        cursor += 4
        return result
    def text():
        global cursor
        length = integer()
        result = payload[cursor:cursor + length].decode("utf-8")
        cursor += length
        return result
    def number():
        global cursor
        result = struct.unpack_from("<d", payload, cursor)[0]
        cursor += 8
        return result
    assert integer() == 4
    samples = []
    for _ in range(4):
        morph, weight, count = text(), number(), integer()
        conditions = [(text(), tuple(number() for _ in range(4)), number()) for _ in range(count)]
        samples.append((morph, weight, conditions))
    assert cursor == len(payload)
    assert [sample[0] for sample in samples] == ["elbow_fix", "elbow_fix", "elbow_fix", "combined"]
    assert [sample[1] for sample in samples] == [1, .6, .8, .8]
    assert [condition[0] for condition in samples[3][2]] == ["elbow", "wrist"]
    for actual, expected in zip(samples[0][2][0][1], (math.sin(math.pi/4), 0, 0, math.cos(math.pi/4))):
        assert math.isclose(actual, expected, abs_tol=1e-12)
    assert math.isclose(samples[0][2][0][2], math.pi/2, abs_tol=1e-12)
    project = (root / "corrective.ii3dproject").read_bytes()
    assert project[:8] == b"iiAOPRJ\0"
    version, length, checksum = struct.unpack_from("<IQI", project, 8)
    assert version == 26 and length == len(project)-24 and checksum == zlib.crc32(project[24:])
    assert project.count(program) == 1
    fbx = read_fbx((root / "corrective.fbx").read_bytes())
    objects = next(node for node in fbx if node[0] == "Objects")[2]
    channels = [node[1][1].split("\0", 1)[0] for node in objects
                if node[0] == "Deformer" and node[1][2] == "BlendShapeChannel"]
    assert channels.count("elbow_fix") == 2 and channels.count("combined") == 2 and channels.count("keep") == 2
    shapes = [node for node in objects if node[0] == "Geometry" and node[1][2] == "Shape"]
    assert len(shapes) == 6
    assert any(any(math.isclose(value, .2, abs_tol=1e-9) for value in child(shape, "Vertices")[1][0]) for shape in shapes)
    glb, binary = read_glb(root / "corrective.glb")
    assert len(glb["meshes"]) == 2
    for mesh in glb["meshes"]:
        names = mesh["extras"]["targetNames"]
        assert names == ["elbow_fix", "combined", "keep"]
        primitive = mesh["primitives"][0]
        deltas = accessor(glb, binary, primitive["targets"][names.index("elbow_fix")]["POSITION"])
        assert sum(math.isclose(delta[2], .2, abs_tol=1e-7) for delta in deltas) == 1
print("Independent corrective program, project envelope, FBX and GLB morph validation passed")
