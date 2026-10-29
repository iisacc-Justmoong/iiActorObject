"""Verify the native envelope independently and exercise CRC-valid malformed payloads."""
import pathlib
import math
import struct
import subprocess
import sys
import zlib

executable, filename = sys.argv[1:]
path = pathlib.Path(filename)
raw = path.read_bytes()
magic, version, size, checksum = struct.unpack_from("<8sIQI", raw)
assert magic == b"iiAOPRJ\0" and version in range(8, 26) and size == len(raw) - 24
payload = raw[24:]
assert zlib.crc32(payload) == checksum
assert struct.unpack_from("<Q", payload)[0] > 0
valid = subprocess.run([executable, "file", str(path)], capture_output=True, text=True)
assert valid.returncode == 0, valid.stderr

def envelope(data, version=version):
    return struct.pack("<8sIQI", magic, version, len(data), zlib.crc32(data)) + data

bad_count = bytearray(payload)
struct.pack_into("<Q", bad_count, 0, 0xffffffffffffffff)
bad_name = bytearray(payload)
struct.pack_into("<Q", bad_name, 8, 0xffffffffffffffff)
variants = [
    envelope(bad_count),
    envelope(bad_name),
    envelope(payload + b"\0"),
    envelope(payload, version=26),
    raw[:-1],
    raw[:20] + struct.pack("<I", checksum ^ 1) + payload,
]
if path.name == "Fixture-v23.ii3dproject":
    name = b"DefaultFixtureMorph"
    marker = struct.pack("<QQ", 1, len(name)) + name + struct.pack("<d", -.375)
    assert version == 23 and payload.count(marker) == 1
    start = payload.index(marker)
    invalid_value = bytearray(payload)
    struct.pack_into("<d", invalid_value, start + 16 + len(name), float("nan"))
    variants.append(envelope(invalid_value))
    invalid_name = bytearray(payload)
    invalid_name[start + 16] = ord("X")
    variants.append(envelope(invalid_name))
if path.name == "Fixture-v19.ii3dproject":
    half_angle = math.atan2(.6, .8) / 2
    candidates = []
    for start in range(len(payload) - 47):
        count, index, x, y, z, w = struct.unpack_from("<QQdddd", payload, start)
        if (count == 1 and index == 0 and x == 0 and z == 0 and
                abs(y - math.sin(half_angle)) < 1e-12 and abs(w - math.cos(half_angle)) < 1e-12):
            candidates.append(start + 16)
    assert len(candidates) == 1, "Expected the single normal correction"
    offset = candidates[0]
    invalid_rotation = bytearray(payload)
    struct.pack_into("<d", invalid_rotation, offset + 24, 5)
    variants.append(envelope(invalid_rotation))
    invalid_index = bytearray(payload)
    struct.pack_into("<Q", invalid_index, offset - 8, 0xffffffff)
    variants.append(envelope(invalid_index))
    duplicate = bytearray(payload)
    struct.pack_into("<Q", duplicate, offset - 16, 2)
    duplicate[offset + 32:offset + 32] = duplicate[offset - 8:offset + 32]
    variants.append(envelope(duplicate))
if version >= 12:
    fairing = struct.pack("<QddQQQ", 8, .7, .05, 16, 4, 1)
    if path.name == "Fixture-v12.ii3dproject":
        assert payload.count(fairing) == 1, "Expected one explicit fairing policy in the v12 fixture"
    if payload.count(fairing) == 1:
        invalid_policy = bytearray(payload)
        offset = payload.index(fairing) + len(fairing) - 8
        struct.pack_into("<Q", invalid_policy, offset, 99)
        variants.append(envelope(invalid_policy))
if version >= 10:
    label = b"Evaluated surface fixture"
    marker = struct.pack("<Q", len(label)) + label
    if payload.count(marker) == 1:
        invalid_evaluation = bytearray(payload)
        offset = payload.index(marker) + len(marker) + 1 + 8
        assert struct.unpack_from("<Q", payload, offset)[0] == 1
        struct.pack_into("<Q", invalid_evaluation, offset, 99)
        variants.append(envelope(invalid_evaluation))
for index, data in enumerate(variants):
    bad = path.parent / f"Malformed-{index}.ii3dproject"
    bad.write_bytes(data)
    result = subprocess.run([executable, "file", str(bad)], capture_output=True, text=True)
    assert result.returncode == 1, (index, result.returncode, result.stderr)
    assert "object.valid()" not in result.stderr and "next.id()" not in result.stderr, result.stderr
print(f"Independent native envelope/CRC verification and {len(variants)} malformed-input rollback checks passed")
