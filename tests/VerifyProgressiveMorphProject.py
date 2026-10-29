"""Independently inspect the v31 envelope and reject malformed program bindings."""
import struct
import subprocess
import sys
import zlib

binary = sys.argv[1]
encoded = subprocess.run([binary, "archive"], text=True, capture_output=True, check=True).stdout.strip()
raw = bytes.fromhex(encoded)
magic, version, length, checksum = struct.unpack_from("<8sIQI", raw)
assert magic == b"iiAOPRJ\0" and version == 31 and length == len(raw) - 24
assert zlib.crc32(raw[24:]) == checksum

def text(value):
    encoded = value.encode("utf-8")
    return struct.pack("<Q", len(encoded)) + encoded

def key(target, weight):
    return text(target) + struct.pack("<d", weight) + text("")

expression = (text("Expression") + struct.pack("<dQ", .15, 5) +
              key("NegativeFar", -.75) + key("NegativeNear", -.2) +
              key("PositiveNear", .3) + key("PositiveMid", .8) + key("PositiveFar", 1) +
              text("") + struct.pack("<Q", 1) + text("Face"))
blink = (text("Blink") + struct.pack("<dQ", 0, 1) + key("Blink", 1) +
         text("") + struct.pack("<Q", 0))
program = struct.pack("<QQ", 1, 2) + expression + blink
assert raw.endswith(program), "Progressive controls, keys and source binding must be retained"

def corrupt(old, new):
    assert len(old) == len(new)
    updated = bytearray(raw)
    index = updated.rfind(text(old))
    assert index >= len(raw) - len(program)
    updated[index + 8:index + 8 + len(old)] = new.encode("utf-8")
    struct.pack_into("<I", updated, 20, zlib.crc32(updated[24:]))
    subprocess.run([binary, "reject_archive"], input=updated.hex(), text=True,
                   capture_output=True, check=True)

corrupt("PositiveNear", "PositiveFake")
corrupt("Face", "Fake")
print("Progressive project envelope, control records and transactional rejection passed")
