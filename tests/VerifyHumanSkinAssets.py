"""Verify the pinned CC0 material inventory and all source image hashes offline."""
import binascii
import hashlib
import json
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1])
manifest = json.loads((root / "UPSTREAM-SHA256.json").read_text())
catalog = json.loads((root / "catalog.json").read_text())
assert catalog["format"] == "iiActorObject.MakeHumanSkin.1" and catalog["baseMesh"] == "hm08"
assert len(catalog["presets"]) == 20 and len(manifest["files"]) == 40
assert len(manifest["archives"]) == 1 and manifest["archives"][0]["etag"]
entries = {entry["local"]: entry for entry in manifest["files"]}
assert len(entries) == len(manifest["files"])
for name, entry in entries.items():
    path = Path(name)
    assert not path.is_absolute() and ".." not in path.parts
    data = (root / path).read_bytes()
    assert len(data) == entry["bytes"]
    assert hashlib.sha256(data).hexdigest() == entry["sha256"]
    assert binascii.crc32(data) & 0xffffffff == entry["zipCrc32"]
    if name.endswith(".png"):
        assert data[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack_from(">II", data, 16) == (2048, 2048)
for preset in catalog["presets"]:
    assert preset["material"] in entries and preset["texture"] in entries
    material = (root / preset["material"]).read_text()
    assert "CC0" in material
    values = [line.split(None, 1)[1].strip() for line in material.splitlines() if line.startswith("diffuseTexture ")]
    assert len(values) == 1 and str(Path(preset["material"]).parent / values[0]) == preset["texture"]
print("Verified 20 CC0 skin materials, image pairs and upstream checksums")
