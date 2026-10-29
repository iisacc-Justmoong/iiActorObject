"""Parse binary FBX material connections independently of the SDK importer."""
import pathlib
import struct
import subprocess
import sys

raw = bytes.fromhex(subprocess.run([sys.argv[1], "normal_fbx", sys.argv[2]], check=True,
                                   text=True, stdout=subprocess.PIPE).stdout.strip())
assert raw[:23] == b"Kaydara FBX Binary  \x00\x1a\x00" and struct.unpack_from("<I", raw, 23)[0] == 7400


def properties(start, count, length):
    end = start + length
    values = []
    for _ in range(count):
        code = chr(raw[start])
        start += 1
        if code in "YCI LFD".replace(" ", ""):
            form = {"Y": "h", "C": "?", "I": "i", "L": "q", "F": "f", "D": "d"}[code]
            values.append(struct.unpack_from("<" + form, raw, start)[0])
            start += struct.calcsize(form)
        elif code in "SR":
            size = struct.unpack_from("<I", raw, start)[0]
            start += 4
            value = raw[start:start + size]
            values.append(value.decode("utf-8") if code == "S" else value)
            start += size
        elif code in "fdilbc":
            _, _, size = struct.unpack_from("<III", raw, start)
            start += 12 + size
            values.append(None)
        else:
            raise AssertionError(f"Unsupported FBX property type {code!r}")
    assert start == end
    return values


def nodes(start, limit):
    result = []
    while start + 13 <= limit:
        end, count, size, name_size = struct.unpack_from("<IIIB", raw, start)
        if not end:
            break
        assert start < end <= limit
        name = raw[start + 13:start + 13 + name_size].decode("ascii")
        begin = start + 13 + name_size
        assert begin + size <= end
        result.append((name, properties(begin, count, size), nodes(begin + size, end)))
        start = end
    return result


def child(node, name):
    matches = [item for item in node[2] if item[0] == name]
    assert len(matches) == 1
    return matches[0]


def user_property(node, key):
    entries = [item[1] for item in child(node, "Properties70")[2]
               if item[0] == "P" and item[1][0] == key]
    assert len(entries) == 1
    return entries[0][-1]


root = nodes(27, len(raw))
objects = next(node for node in root if node[0] == "Objects")[2]
connections = next(node for node in root if node[0] == "Connections")[2]
materials = {node[1][0]: node for node in objects if node[0] == "Material"}
textures = {node[1][0]: node for node in objects if node[0] == "Texture"}
videos = {node[1][0]: node for node in objects if node[0] == "Video"}
links = [node[1] for node in connections if node[0] == "C"]
normal_links = [link for link in links if len(link) == 4 and link[0] == "OP" and link[3] == "NormalMap"]
assert len(normal_links) == 2
expected = {"Surface": ("LightmapUV", .25), "Second": ("SurfaceUV", -.5)}
for _, texture_id, material_id, _ in normal_links:
    material = materials[material_id]
    name = material[1][1].split("\x00", 1)[0]
    uv, scale = expected.pop(name)
    assert user_property(material, "iiNormalScale") == scale
    assert user_property(textures[texture_id], "UVSet") == uv
    linked_videos = [link[1] for link in links if len(link) == 3 and link[0] == "OO" and link[2] == texture_id and link[1] in videos]
    assert len(linked_videos) == 1
    assert child(videos[linked_videos[0]], "Content")[1][0] == (pathlib.Path(sys.argv[2]).parent / "blue.png").read_bytes()
assert not expected
print("Independent FBX normal-map, UV, custom scale and embedded bytes passed")
