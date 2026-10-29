"""Inspect the grouped-aim project header and record without the SDK decoder."""
import struct
import subprocess
import sys
import zlib

encoded = subprocess.run([sys.argv[1], "group_archive"], text=True,
                         capture_output=True, check=True).stdout.strip()
data = bytes.fromhex(encoded)
magic, version, length, checksum = struct.unpack_from("<8sIQI", data)
assert magic == b"iiAOPRJ\0" and version == 30 and length == len(data) - 24
assert zlib.crc32(data[24:]) == checksum
# Existing joint ID, primary descendant and target, followed by one extra anchor.
record = struct.pack("<QQdddQQ", 4, 1, 1.0, 0.0, 0.0, 1, 2)
assert record in data[24:], "Grouped aim and its second anchor must be serialized"
print("Grouped aim project version, length, checksum and anchor record passed")
