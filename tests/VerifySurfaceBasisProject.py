"""Check the new project basis envelope and reject corrupt basis matrices."""
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import zlib

executable, fixture = sys.argv[1:]
raw = pathlib.Path(fixture).read_bytes()
magic, version, length, checksum = struct.unpack_from("<8sIQI", raw)
assert magic == b"iiAOPRJ\0" and version == 25 and length == len(raw) - 24
assert checksum == zlib.crc32(raw[24:])
basis = b"\x01" + struct.pack("<9d", 2, 0, 0, 0, 3, 0, 0, 0, 4)
assert raw.count(basis) == 1
start = raw.index(basis) + 1

def envelope(data):
    struct.pack_into("<I", data, 20, zlib.crc32(data[24:]))
    return data

variants = []
singular = bytearray(raw)
struct.pack_into("<3d", singular, start, 0, 0, 0)
variants.append(envelope(singular))
nonfinite = bytearray(raw)
struct.pack_into("<d", nonfinite, start, float("nan"))
variants.append(envelope(nonfinite))
ill_conditioned = bytearray(raw)
struct.pack_into("<d", ill_conditioned, start, 1e-14)
variants.append(envelope(ill_conditioned))
unknown = bytearray(raw)
struct.pack_into("<I", unknown, 8, 26)
variants.append(unknown)

with tempfile.TemporaryDirectory(prefix="surface-basis-", dir=os.getcwd()) as directory:
    path = pathlib.Path(directory) / "test.ii3dproject"
    path.write_bytes(raw)
    subprocess.run([executable, "basis_project_valid", str(path)], check=True, capture_output=True)
    for variant in variants:
        path.write_bytes(variant)
        subprocess.run([executable, "basis_project_reject", str(path)], check=True, capture_output=True)
print("Project v25 basis and malformed input checks passed")
