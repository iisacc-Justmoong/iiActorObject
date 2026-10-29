"""Check the v28 envelope and corrupt history independently of the SDK codec."""
import struct
import subprocess
import sys
import zlib

executable = sys.argv[1]
raw = bytes.fromhex(subprocess.run([executable, "encoded"], check=True,
                                   capture_output=True, text=True).stdout.strip())
magic, version, length, checksum = struct.unpack_from("<8sIQI", raw)
assert magic == b"iiAOPRJ\0" and version == 28 and length == len(raw) - 24
payload = raw[24:]
assert zlib.crc32(payload) == checksum
marker = struct.pack("<Q", 4) + b"Face"
assert payload.count(marker) >= 2


def wrap(data, selected_version=28):
    return struct.pack("<8sIQI", magic, selected_version, len(data), zlib.crc32(data)) + data


def probe(value):
    result = subprocess.run([executable, "probe"], input=value.hex(), capture_output=True, text=True)
    assert result.returncode in (0, 1) and not result.stderr, result.stderr
    return result.returncode


assert probe(raw) == 0
bad_name = bytearray(payload)
bad_name[bad_name.rindex(marker) + 8] = ord("G")
bad_size = bytearray(payload)
struct.pack_into("<Q", bad_size, 0, 0xffffffffffffffff)
variants = [wrap(payload, 27), wrap(payload, 29), wrap(bad_name), wrap(bad_size),
            wrap(payload + b"\0"), raw[:-1], raw[:20] + struct.pack("<I", checksum ^ 1) + payload]
for item in variants:
    assert probe(item) == 1
print(f"Independent v28 envelope and {len(variants)} rejected inputs verified")
