"""Independently inspect project animation v27 and submit CRC-valid malformed records."""
import math
import struct
import subprocess
import sys
import zlib

executable = sys.argv[1]
encoded = subprocess.run([executable, "encoded"], check=True, capture_output=True, text=True).stdout.strip()
raw = bytes.fromhex(encoded)
magic, version, size, checksum = struct.unpack_from("<8sIQI", raw)
assert magic == b"iiAOPRJ\0" and version == 27 and size == len(raw) - 24
payload = raw[24:]
assert zlib.crc32(payload) == checksum
marker = struct.pack("<QQ", 1, len(b"take_alpha")) + b"take_alpha"
assert payload.count(marker) == 1
pos = payload.index(marker)


def integer():
    global pos
    value = struct.unpack_from("<Q", payload, pos)[0]
    pos += 8
    return value


def number():
    global pos
    value = struct.unpack_from("<d", payload, pos)[0]
    pos += 8
    return value


def string():
    global pos
    length = integer()
    value = payload[pos:pos + length].decode("utf-8")
    pos += length
    return value


assert integer() == 1
name_offset = pos + 8
assert string() == "take_alpha" and number() == 1
assert integer() == 1 and integer() == 1 and integer() == 2
second_key_time = None
for i in range(2):
    if i == 1:
        second_key_time = pos
    seconds = number()
    position = [number() for _ in range(3)]
    rotation = [number() for _ in range(4)]
    scale = [number() for _ in range(3)]
    assert seconds == i and position == [i * 2, 1, 0]
    assert rotation == [0, 0, 0, 1] and scale == [1, 1, 1]
assert integer() == 2
for name, values in (("left", (0., 1.)), ("right", (1., 0.))):
    assert string() == name and integer() == 2
    assert [(number(), number()) for _ in range(2)] == [(0., values[0]), (1., values[1])]
assert integer() == 0
assert integer() == 2
bindings = []
for expected in ((0, 0, 0), (1, 1, 0)):
    binding_offset = pos
    assert (integer(), integer(), integer()) == expected
    bindings.append(binding_offset)
flag_offset = pos
assert payload[pos] == 1
pos += 1
source_offset = pos
assert number() == -2.75 and pos == len(payload)


def probe(data):
    result = subprocess.run([executable, "probe"], input=data.hex(), capture_output=True, text=True)
    assert result.returncode in (0, 1), result.stderr
    return result.returncode


def wrap(data):
    return struct.pack("<8sIQI", magic, version, len(data), zlib.crc32(data)) + data


assert probe(raw) == 0
variants = []
for offset, value in ((bindings[0], 9), (bindings[0] + 16, 9), (bindings[1] + 8, 0)):
    edited = bytearray(payload)
    struct.pack_into("<Q", edited, offset, value)
    variants.append(edited)
edited = bytearray(payload)
struct.pack_into("<d", edited, second_key_time, 0.)
variants.append(edited)
edited = bytearray(payload)
struct.pack_into("<d", edited, source_offset, math.nan)
variants.append(edited)
edited = bytearray(payload)
edited[name_offset] = 0
variants.append(edited)
edited = bytearray(payload)
edited[flag_offset] = 2
variants.append(edited)
for edited in variants:
    assert probe(wrap(edited)) == 1
print(f"Independent v27 record inspection and {len(variants)} CRC-valid rejection checks passed")
