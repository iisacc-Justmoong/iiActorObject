"""Actual Blender import and visible garment skin deformation; independent of SDK runtime."""
from pathlib import Path
import json
import math
import sys
import struct
import bpy
from mathutils import Quaternion

directory = Path(sys.argv[sys.argv.index("--") + 1])
results = []
for filename in ("ClothedHuman.glb", "ClothedHuman.fbx"):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = directory / filename
    if path.suffix == ".glb":
        raw = path.read_bytes()
        size = struct.unpack_from("<I", raw, 12)[0]
        assert len(json.loads(raw[20:20 + size])["meshes"]) == 2
        bpy.ops.import_scene.gltf(filepath=str(path))
    else:
        bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=True)
    rigs = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    # Blender may create an unskinned Icosphere bone widget during glTF import.
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"
              and any(m.type == "ARMATURE" for m in o.modifiers)]
    assert len(rigs) == 1 and len(rigs[0].data.bones) == 163
    assert len(meshes) == 2, [(o.name, len(o.data.polygons)) for o in meshes]
    garment = next(o for o in meshes if "GarmentProxy" in o.name or "GarmentProxy" in o.data.name)
    assert len(garment.data.polygons) > 300
    visible = {v for p in garment.data.polygons for v in p.vertices}
    assert len(visible) > 500
    graph = bpy.context.evaluated_depsgraph_get()
    def positions(obj):
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        values = [evaluated.matrix_world @ v.co for v in mesh.vertices]
        evaluated.to_mesh_clear()
        return values
    rest = positions(garment)
    original = [garment.matrix_world @ v.co for v in garment.data.vertices]
    rest_error = max((rest[i] - original[i]).length for i in visible)
    assert rest_error < 2e-5, (filename, rest_error)
    for i in visible:
        assert garment.data.vertices[i].groups
        total = sum(w.weight for w in garment.data.vertices[i].groups)
        assert abs(total - 1) < 2e-5, (filename, i, total)
    bone = rigs[0].pose.bones["spine02"]
    bone.rotation_mode = "QUATERNION"
    bone.rotation_quaternion = bone.rotation_quaternion @ Quaternion((0, 0, 1), .25)
    bpy.context.view_layer.update()
    posed = positions(garment)
    movement = max((posed[i] - rest[i]).length for i in visible)
    assert movement > .01 and all(math.isfinite(c) for i in visible for c in posed[i])
    results.append({"file": filename, "bones": 163, "garmentVisibleVertices": len(visible),
                    "garmentPolygons": len(garment.data.polygons), "bindErrorMetres": rest_error,
                    "visibleSkinMovementMetres": movement})
report = {"blender": bpy.app.version_string, "results": results}
(directory / "BlenderSurfaceVerification.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))
