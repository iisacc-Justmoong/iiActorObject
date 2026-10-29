"""Blender IO adapter. Core authoring/merge algorithms remain in the C++23 SDK."""
import shlex
from pathlib import Path
import bpy

DOCUMENT = "iiActorObjectDocument"
POINTS = "iiActorObjectSourcePointIds"
POLYGONS = "iiActorObjectSourcePolygonIds"
CORNERS = "iiActorObjectSourceCornerLayouts"
MORPHS = "iiActorObjectSourceMorphs"
BONE = "iiActorObjectSourceBone"
MATERIAL = "iiActorObjectSourceMaterial"
MESH = "iiActorObjectSourceMesh"
POINT_ATTRIBUTE = "iiActorObjectSourcePoint"
FACE_ATTRIBUTE = "iiActorObjectSourcePolygon"
CORNER_ATTRIBUTE = "iiActorObjectSourceLayout"
MORPH_PROPERTY = "iiActorObjectSourceMorph"


def _ids(text, count):
    result = [] if not text else [int(value) for value in str(text).split(",")]
    if len(result) != count or any(value < 0 or value > 2147483647 for value in result):
        raise ValueError("Source ID dimensions or range changed")
    return result


def _morphs(text):
    words = shlex.split(str(text))
    if len(words) < 3 or words[:2] != ["iiActorObjectMorphNames", "1"]:
        raise ValueError("Invalid source morph identities")
    count = int(words[2])
    if count < 0 or count > 4096 or len(words) != 3 + 2 * count:
        raise ValueError("Invalid source morph identity count")
    names = dict(zip(words[3::2], words[4::2]))
    if len(names) != count or len(set(names.values())) != count:
        raise ValueError("Duplicate source morph identities")
    return names


def _quoted(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _morph_identity(key):
    try:
        return key.get(MORPH_PROPERTY)
    except (AttributeError, TypeError):
        return None


class ExternalSession:
    """Preserve source attributes in a scene and export baseline/edited FBX snapshots."""

    def __init__(self, document=None):
        tokens = {obj[DOCUMENT] for obj in bpy.context.scene.objects if DOCUMENT in obj}
        if document is None:
            if len(tokens) != 1:
                raise ValueError("Choose one external document in the scene")
            document = next(iter(tokens))
        if document not in tokens or not document or len(document.encode()) > 128:
            raise ValueError("External document is missing or invalid")
        self.document = document
        self.objects = [obj for obj in bpy.context.scene.objects if obj.get(DOCUMENT) == document]
        self.meshes = [obj for obj in self.objects if obj.type == "MESH" and MESH in obj]
        self.rigs = [obj for obj in self.objects if obj.type == "ARMATURE"]
        if not self.meshes or len(self.rigs) > 1:
            raise ValueError("Expected one source asset and at most one armature")
        self._morph_bindings = {}
        mesh_ids = [int(obj[MESH]) for obj in self.meshes]
        if len(set(mesh_ids)) != len(mesh_ids):
            raise ValueError("Duplicate source mesh IDs")
        for rig in self.rigs:
            for bone in rig.data.bones:
                if BONE not in bone and BONE in rig.pose.bones[bone.name]:
                    bone[BONE] = int(rig.pose.bones[bone.name][BONE])
                if BONE not in bone:
                    raise ValueError("A bone has no source identity: " + bone.name)
        for obj in self.meshes:
            mesh = obj.data
            for prop, name, domain, count in ((POINTS, POINT_ATTRIBUTE, "POINT", len(mesh.vertices)),
                                               (POLYGONS, FACE_ATTRIBUTE, "FACE", len(mesh.polygons)),
                                               (CORNERS, CORNER_ATTRIBUTE, "CORNER", len(mesh.loops))):
                attribute = mesh.attributes.get(name)
                if attribute is None:
                    values = _ids(obj[prop], count)
                    attribute = mesh.attributes.new(name, "INT", domain)
                    for item, value in zip(attribute.data, values):
                        item.value = value
                if attribute.domain != domain or attribute.data_type != "INT" or len(attribute.data) != count:
                    raise ValueError("Source attribute type or dimensions changed")
            names = _morphs(obj[MORPHS])
            bindings = {}
            if mesh.shape_keys:
                for key in list(mesh.shape_keys.key_blocks)[1:]:
                    source = _morph_identity(key)
                    source = source or names.get(key.name)
                    if source is None:
                        raise ValueError("Missing source morph identity: " + key.name)
                    bindings[key.as_pointer()] = source
                    try:
                        key[MORPH_PROPERTY] = source
                    except TypeError:
                        pass
            self._morph_bindings[int(obj[MESH])] = bindings

    @classmethod
    def import_fbx(cls, path, document=None):
        existing = {obj.as_pointer() for obj in bpy.context.scene.objects}
        if bpy.ops.import_scene.fbx(filepath=str(Path(path).resolve()), use_custom_props=True) != {"FINISHED"}:
            raise RuntimeError("FBX import failed")
        # FBX shininess is interpreted differently by applications. Restore the
        # SDK's authored PBR scalars once on import, before capturing a baseline.
        # Attaching to an already-open scene must not overwrite unsent edits.
        for obj in bpy.context.scene.objects:
            if obj.as_pointer() in existing or obj.type != "MESH" or MESH not in obj:
                continue
            for material in obj.data.materials:
                if material is None:
                    continue
                shader = next((node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED"), None) if material.use_nodes and material.node_tree else None
                for prop, field, socket in (("iiRoughness", "roughness", "Roughness"),
                                            ("iiMetallic", "metallic", "Metallic")):
                    if prop in material:
                        value = float(material[prop])
                        if not 0 <= value <= 1:
                            raise ValueError("Invalid authored PBR scalar")
                        setattr(material, field, value)
                        if shader:
                            shader.inputs[socket].default_value = value
        return cls(document)

    def _update_properties(self):
        for obj in self.meshes:
            for prop, attribute in ((POINTS, POINT_ATTRIBUTE), (POLYGONS, FACE_ATTRIBUTE),
                                    (CORNERS, CORNER_ATTRIBUTE)):
                values = [item.value for item in obj.data.attributes[attribute].data]
                if any(value < 0 for value in values):
                    raise ValueError("Invalid source IDs")
                if prop != CORNERS and len(set(values)) != len(values):
                    raise ValueError("A topology change needs an explicit source remap")
                obj[prop] = ",".join(map(str, values))
            names, used = [], set()
            if obj.data.shape_keys:
                bindings = self._morph_bindings[int(obj[MESH])]
                for key in list(obj.data.shape_keys.key_blocks)[1:]:
                    source = _morph_identity(key)
                    source = source or bindings.get(key.as_pointer()) or key.name
                    if source in used:
                        raise ValueError("Duplicate source morph identity")
                    used.add(source)
                    names.append((key.name, source))
                    bindings[key.as_pointer()] = source
            obj[MORPHS] = "iiActorObjectMorphNames 1 " + str(len(names)) + "\n" + "".join(
                _quoted(name) + " " + _quoted(source) + "\n" for name, source in names)
            for material in obj.data.materials:
                if material is None or MATERIAL not in material:
                    raise ValueError("A material needs an explicit source identity")
                principled = next((node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED"), None) if material.use_nodes and material.node_tree else None
                if "iiRoughness" in material:
                    material["iiRoughness"] = principled.inputs["Roughness"].default_value if principled else material.roughness
                if "iiMetallic" in material:
                    material["iiMetallic"] = principled.inputs["Metallic"].default_value if principled else material.metallic

    def export_fbx(self, path):
        selected = list(bpy.context.selected_objects)
        active = bpy.context.view_layer.objects.active
        mode = active.mode if active else "OBJECT"
        try:
            if active and mode != "OBJECT":
                bpy.ops.object.mode_set(mode="OBJECT")
            self._update_properties()
            bpy.ops.object.select_all(action="DESELECT")
            for obj in self.objects:
                obj.select_set(True)
            bpy.context.view_layer.objects.active = self.rigs[0] if self.rigs else self.meshes[0]
            result = bpy.ops.export_scene.fbx(filepath=str(Path(path).resolve()), use_selection=True,
                                             add_leaf_bones=False, bake_anim=False, use_mesh_modifiers=False,
                                             use_custom_props=True, path_mode="COPY", embed_textures=True, axis_forward="-Z", axis_up="Y")
            if result != {"FINISHED"}:
                raise RuntimeError("FBX export failed")
        finally:
            bpy.ops.object.select_all(action="DESELECT")
            for obj in selected:
                if obj.name in bpy.context.view_layer.objects:
                    obj.select_set(True)
            if active and active.name in bpy.context.view_layer.objects:
                bpy.context.view_layer.objects.active = active
                if mode != "OBJECT":
                    bpy.ops.object.mode_set(mode=mode)
