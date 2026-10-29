"""Check actual embedded base-color image connections and original PNG bytes."""
from pathlib import Path
import hashlib
import json
import sys
import bpy

directory, original = map(Path, sys.argv[sys.argv.index("--") + 1:])
digest = hashlib.sha256(original.read_bytes()).hexdigest()
results = []
for extension in ("glb", "fbx"):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = str(directory / ("Checker." + extension))
    result = bpy.ops.import_scene.gltf(filepath=path) if extension == "glb" else bpy.ops.import_scene.fbx(filepath=path)
    assert result == {"FINISHED"}
    material = next(material for material in bpy.data.materials if material.use_nodes)
    nodes = [node for node in material.node_tree.nodes if node.type == "TEX_IMAGE" and node.image]
    assert nodes and all(tuple(node.image.size) == (8, 8) for node in nodes), (extension, material.name)
    color_nodes = [node for node in nodes if any(link.from_node == node and link.to_socket.name == "Base Color" for link in material.node_tree.links)]
    assert len(color_nodes) == 1, (extension, material.name)
    assert any(link.to_socket.name == "Alpha" and link.from_socket.name == "Alpha" for link in material.node_tree.links), (extension, "PNG alpha is not connected")
    # FBX import creates a second node for an RGBA image's alpha channel.
    for node in nodes:
        imported = node.image
        encoded = bytes(imported.packed_file.data) if imported.packed_file else Path(bpy.path.abspath(imported.filepath)).read_bytes()
        assert hashlib.sha256(encoded).hexdigest() == digest
    image = color_nodes[0].image
    data = bytes(image.packed_file.data) if image.packed_file else Path(bpy.path.abspath(image.filepath)).read_bytes()
    assert hashlib.sha256(data).hexdigest() == digest
    pixels = list(image.pixels)
    red = sum(pixels[i] > .99 and pixels[i+1] < .01 for i in range(0, len(pixels), 4))
    green = sum(pixels[i] < .01 and pixels[i+1] > .99 for i in range(0, len(pixels), 4))
    assert red == 32 and green == 32
    results.append({"format": extension, "imageNodes": len(nodes), "imageSha256": digest, "redPixels": red, "greenPixels": green})
(directory / "BlenderImagesVerification.json").write_text(json.dumps({"blender": bpy.app.version_string, "results": results}, indent=2) + "\n")
print(json.dumps(results))
