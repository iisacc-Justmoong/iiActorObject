"""Independently verify the offline source inventory and component references."""
import binascii
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[1])
manifest = json.loads((root / "UPSTREAM-SHA256.json").read_text())
catalog = json.loads((root / "catalog.json").read_text())
assert catalog["format"] == "iiActorObject.MakeHumanWardrobe.1" and catalog["baseMesh"] == "hm08"
assert len(catalog["components"]) == 6 and len(manifest["files"]) == 28
assert len(manifest["archives"]) == 1 and manifest["archives"][0]["etag"]
files = {item["local"]: item for item in manifest["files"]}
assert len(files) == len(manifest["files"])
for name, entry in files.items():
    path = Path(name)
    assert not path.is_absolute() and ".." not in path.parts
    content = (root / path).read_bytes()
    assert len(content) == entry["bytes"]
    assert hashlib.sha256(content).hexdigest() == entry["sha256"]
    assert binascii.crc32(content) & 0xffffffff == entry["zipCrc32"]
    if name.endswith((".mhclo", ".mhmat")):
        assert b"CC0" in content
    if name.endswith(".png"):
        assert content[:8] == b"\x89PNG\r\n\x1a\n"
ids, names = set(), set()
for item in catalog["components"]:
    assert item["id"] not in ids and item["name"] not in names
    ids.add(item["id"]); names.add(item["name"])
    proxy = Path(item["proxy"])
    assert str(proxy) in files
    records = (root / proxy).read_text().splitlines()
    properties = {}
    for record in records:
        parts = record.split()
        if parts and parts[0] in ("basemesh", "obj_file", "material"):
            assert len(parts) == 2 and parts[0] not in properties
            properties[parts[0]] = parts[1]
    assert properties["basemesh"] == "hm08"
    mesh, material = (proxy.parent / properties[key] for key in ("obj_file", "material"))
    assert str(mesh) in files and str(material) in files
    for record in (root / material).read_text().splitlines():
        parts = record.split()
        if parts and parts[0] in ("diffuseTexture", "normalmapTexture", "aomapTexture"):
            assert len(parts) == 2 and str(material.parent / parts[1]) in files
print("Verified six CC0 components, referenced source files and 28 upstream checksums")
