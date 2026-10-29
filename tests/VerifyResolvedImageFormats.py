"""Independently inspect converted image payloads and links in generated GLB and FBX."""
import json
import struct
import subprocess
import sys
import zlib

from FbxBinaryReader import child, read_fbx


def produce(mode):
    command = [sys.argv[1], mode, *sys.argv[2:5]]
    return bytes.fromhex(subprocess.run(command, check=True, capture_output=True, text=True).stdout)


def verify_png(raw):
    assert raw[:8] == b"\x89PNG\r\n\x1a\n"
    offset = 8
    compressed = b""
    size = None
    while offset < len(raw):
        length = struct.unpack_from(">I", raw, offset)[0]
        kind = raw[offset + 4:offset + 8]
        payload = raw[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", raw, offset + 8 + length)[0]
        assert zlib.crc32(kind + payload) == crc
        if kind == b"IHDR":
            size = struct.unpack_from(">II", payload)
        if kind == b"IDAT":
            compressed += payload
        offset += length + 12
        if kind == b"IEND":
            break
    assert offset == len(raw) and size == (2, 2) and zlib.decompress(compressed)


glb = produce("independent_glb")
assert struct.unpack_from("<III", glb) == (0x46546C67, 2, len(glb))
json_size, json_kind = struct.unpack_from("<II", glb, 12)
assert json_kind == 0x4E4F534A
doc = json.loads(glb[20:20 + json_size])
bin_at = 20 + json_size
bin_size, bin_kind = struct.unpack_from("<II", glb, bin_at)
assert bin_kind == 0x004E4942 and bin_at + 8 + bin_size == len(glb)
binary = glb[bin_at + 8:]
images = []
for item in doc["images"]:
    assert item["mimeType"] == "image/png"
    view = doc["bufferViews"][item["bufferView"]]
    assert view["buffer"] == 0
    start = view.get("byteOffset", 0)
    image = binary[start:start + view["byteLength"]]
    verify_png(image)
    images.append(image)
assert len(images) == 2
material = doc["materials"][0]
base = material["pbrMetallicRoughness"]["baseColorTexture"]["index"]
normal = material["normalTexture"]["index"]
assert {doc["textures"][base]["source"], doc["textures"][normal]["source"]} == {0, 1}

fbx = produce("independent_fbx")
root = read_fbx(fbx)
objects = next(node for node in root if node[0] == "Objects")[2]
connections = next(node for node in root if node[0] == "Connections")[2]
videos = [node for node in objects if node[0] == "Video"]
fbx_images = [child(video, "Content")[1][0] for video in videos]
assert len(fbx_images) == 2 and sorted(fbx_images) == sorted(images)
for image in fbx_images:
    verify_png(image)
links = [node[1] for node in connections if node[0] == "C"]
assert {link[3] for link in links if len(link) == 4 and link[0] == "OP"} >= {"DiffuseColor", "NormalMap"}
print("Independent GLB and FBX converted image payloads and material links passed")
