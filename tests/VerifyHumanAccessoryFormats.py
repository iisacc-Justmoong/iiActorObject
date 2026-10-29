"""Inspect GLB and FBX produced from the authored CC0 accessory data."""
from pathlib import Path
import subprocess
import sys
import tempfile

from FbxBinaryReader import child, read_fbx
from VerifyCharacterGlb import read_glb

exe, body, accessory, build = sys.argv[1:5]
switched = len(sys.argv) == 6 and sys.argv[5] == "--switch"
removed = len(sys.argv) == 6 and sys.argv[5] == "--remove"
added = len(sys.argv) == 6 and sys.argv[5] == "--add"
mixed = len(sys.argv) == 6 and sys.argv[5] == "--mixed"
assert len(sys.argv) in (5, 6) and (len(sys.argv) == 5 or switched or removed or added or mixed)
body, accessory = Path(body), Path(accessory)
hair_name = "HairShort01" if switched else "HairBob01"
suit_name = "MaleCasualSuit01" if switched else "FemaleCasualSuit01"
hair_stem = "short01" if switched else "bob01"
suit_stem = "male_casualsuit01" if switched else "female_casualsuit01"
source = {
    hair_name + "Color": f"hair/{hair_stem}/{hair_stem}_diffuse.png",
    suit_name + "Color": f"clothes/{suit_stem}/{suit_stem}_diffuse.png",
    suit_name + "Normal": f"clothes/{suit_stem}/{suit_stem}_normal.png",
    "Eyebrows001Color": "eyebrows/eyebrow001/eyebrow001.png",
    "Eyelashes01Color": "eyelashes/eyelashes01/eyelashes01.png",
}
expected_images = {name: (accessory / path).read_bytes() for name, path in source.items()}
if mixed:
    expected_images["HairShort01Color"] = (accessory / "hair/short01/short01_diffuse.png").read_bytes()

words = [] if removed or mixed else (accessory / f"clothes/{suit_stem}/{suit_stem}.mhclo").read_text().split("delete_verts", 1)[1].split()
hidden = set(); i = 0
while i < len(words):
    start = end = int(words[i]); i += 1
    if i < len(words) and words[i] == "-":
        end = int(words[i + 1]); i += 2
    hidden.update(range(start, end + 1))
group = set(); visible_triangles = hidden_faces = 0
for line in (body / "base.obj").read_text().splitlines():
    tokens = line.split()
    if not tokens:
        continue
    if tokens[0] == "g":
        group = set(tokens[1:])
    elif tokens[0] == "f" and "body" in group:
        points = [int(value.split("/")[0]) - 1 for value in tokens[1:]]
        if all(point in hidden for point in points):
            hidden_faces += 1
        else:
            visible_triangles += len(points) - 2
if removed or mixed:
    assert hidden_faces == 0 and visible_triangles == 26756
elif not switched:
    assert hidden_faces == 2688 and visible_triangles == 21380
else:
    assert hidden_faces > 0 and visible_triangles > 0

with tempfile.TemporaryDirectory(prefix="accessory-formats-", dir=build) as temporary:
    mode = "mixed_formats" if mixed else "add_formats" if added else "remove_formats" if removed else "switch_formats" if switched else "formats"
    subprocess.run([exe, mode, str(body), str(accessory), temporary], check=True)
    document, binary = read_glb(Path(temporary) / "character.glb")
    meshes = {mesh["name"]: mesh for mesh in document["meshes"]}
    assert set(meshes) == ({"OBJ", "HairShort01", "Eyebrows001", "Eyelashes01"} if mixed else
                           {"OBJ", hair_name, "Eyebrows001", "Eyelashes01"} if removed else
                           {"OBJ", hair_name, suit_name, "Eyebrows001", "Eyelashes01"})
    triangles = sum(document["accessors"][p["indices"]]["count"] // 3 for p in meshes["OBJ"]["primitives"])
    assert triangles == visible_triangles
    assert all("JOINTS_0" in primitive["attributes"] and "WEIGHTS_0" in primitive["attributes"]
               for mesh in meshes.values() for primitive in mesh["primitives"])
    images = {}
    for image in document["images"]:
        view = document["bufferViews"][image["bufferView"]]; start = view.get("byteOffset", 0)
        images[image["name"]] = binary[start:start + view["byteLength"]]
    assert images == expected_images
    materials = {material["name"]: material for material in document["materials"]}
    hair, suit = materials[("HairShort01" if mixed else hair_name) + "Material"], materials[suit_name + "Material"]
    assert hair["alphaMode"] == "BLEND" and hair["doubleSided"] is True
    assert suit["alphaMode"] == "OPAQUE" and not suit.get("doubleSided", False)
    normal = suit["normalTexture"]
    normal_image = document["images"][document["textures"][normal["index"]]["source"]]
    assert images[normal_image["name"]] == expected_images[suit_name + "Normal"]
    assert normal["scale"] == 1

    nodes = read_fbx((Path(temporary) / "character.fbx").read_bytes())
    objects = next(node[2] for node in nodes if node[0] == "Objects")
    videos = [child(node, "Content")[1][0] for node in objects if node[0] == "Video"]
    assert sorted(videos) == sorted(expected_images.values())
    links = [node[1] for node in next(node[2] for node in nodes if node[0] == "Connections") if node[0] == "C"]
    assert sum(len(link) == 4 and link[0] == "OP" and link[3] == "DiffuseColor" for link in links) == (5 if mixed else 4)
    assert sum(len(link) == 4 and link[0] == "OP" and link[3] == "NormalMap" for link in links) == 1
    geometry = [node for node in objects if node[0] == "Geometry" and node[1][2] == "Mesh"]
    assert len(geometry) == (4 if removed or mixed else 5)
    skins = {node[1][0]: node for node in objects if node[0] == "Deformer" and node[1][2] == "Skin"}
    clusters = {node[1][0]: node for node in objects if node[0] == "Deformer" and node[1][2] == "Cluster"}
    bones = {node[1][0] for node in objects if node[0] == "Model" and node[1][2] == "LimbNode"}
    object_links = [(link[1], link[2]) for link in links if len(link) == 3 and link[0] == "OO"]
    for mesh in geometry:
        point_count = len(child(mesh, "Vertices")[1][0]) // 3
        attached = [skin for skin in skins if (skin, mesh[1][0]) in object_links]
        assert len(attached) == 1
        bound = [cluster for cluster in clusters if (cluster, attached[0]) in object_links]
        assert bound
        totals = [0.] * point_count
        for cluster in bound:
            assert any((bone, cluster) in object_links for bone in bones)
            indices = child(clusters[cluster], "Indexes")[1][0]
            weights = child(clusters[cluster], "Weights")[1][0]
            assert len(indices) == len(weights) and indices
            for index, weight in zip(indices, weights):
                assert 0 <= index < point_count and weight > 0
                totals[index] += weight
        assert all(abs(total - 1) < 1e-8 for total in totals)
print("Independent GLB/FBX accessory images, skin, materials and covered body faces passed; hidden=",hidden_faces,"visible_triangles=",visible_triangles,"switched=",switched,"removed=",removed,"added=",added,"mixed=",mixed)
