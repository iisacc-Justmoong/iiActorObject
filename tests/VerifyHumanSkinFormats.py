"""Read generated GLB/FBX independently and compare the source image and bindings."""
import json
import pathlib
import subprocess
import sys
import tempfile

from FbxBinaryReader import child, read_fbx
from VerifyCharacterGlb import accessor, read_glb

exe, body, skin, build = sys.argv[1:]
root = pathlib.Path(skin)
catalog = json.loads((root / "catalog.json").read_text())
entry = next(p for p in catalog["presets"] if p["id"] == "young_african_male")
original = (root / entry["texture"]).read_bytes()

with tempfile.TemporaryDirectory(prefix="human-skin-formats-", dir=build) as temp:
    subprocess.run([exe, "formats", body, skin, temp], check=True)
    document, binary = read_glb(pathlib.Path(temp) / "skin.glb")
    primitive = document["meshes"][0]["primitives"][0]
    material = document["materials"][primitive["material"]]
    texture = material["pbrMetallicRoughness"]["baseColorTexture"]
    image = document["images"][document["textures"][texture["index"]]["source"]]
    view = document["bufferViews"][image["bufferView"]]
    start = view.get("byteOffset", 0)
    assert image["mimeType"] == "image/png" and binary[start:start + view["byteLength"]] == original
    uvs = accessor(document, binary, primitive["attributes"]["TEXCOORD_0"])
    assert len(uvs) == 3 and {tuple(v) for v in uvs} == {(0, 1), (1, 1), (0, 0)}
    assert material["pbrMetallicRoughness"]["baseColorFactor"] == [1, 1, 1, 1]
    assert abs(material["pbrMetallicRoughness"]["roughnessFactor"] - .55) < 1e-6

    fbx = read_fbx((pathlib.Path(temp) / "skin.fbx").read_bytes())
    objects = next(node for node in fbx if node[0] == "Objects")[2]
    connections = next(node for node in fbx if node[0] == "Connections")[2]
    videos = [node for node in objects if node[0] == "Video"]
    assert len(videos) == 1 and child(videos[0], "Content")[1][0] == original
    links = [node[1] for node in connections if node[0] == "C"]
    assert any(len(link) == 4 and link[0] == "OP" and link[3] == "DiffuseColor" for link in links)
print("Independent GLB and FBX skin image and material checks passed")
