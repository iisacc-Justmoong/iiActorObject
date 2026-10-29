"""Check complete ownership classification and project header boundaries."""
import json
import re
import sys
from pathlib import Path, PurePosixPath

project = Path(sys.argv[1]).resolve()
manifest = json.loads((project / "resources/architecture/ResponsibilityMap.json").read_text(encoding="utf-8"))
if manifest.get("schema_version") != 1 or manifest.get("source_root") != "src":
    raise AssertionError("Unexpected responsibility map schema or source root")
root = (project / manifest["source_root"]).resolve()
if (root / "iiActorObject").exists():
    raise AssertionError("Source root must not repeat the project name")
modules = manifest["modules"]
ids = set()
listed = {}
owners = {}
for module in modules:
    name, owner = module["id"], module["owner"]
    if name in ids or owner not in ("core", "character") or not module["responsibility"]:
        raise AssertionError(f"Invalid responsibility group: {name}")
    ids.add(name)
    owners[name] = owner
    for raw in module["files"]:
        path = PurePosixPath(raw)
        if path.is_absolute() or ".." in path.parts or path.suffix not in (".h", ".cpp"):
            raise AssertionError(f"Invalid source path: {raw}")
        if raw in listed:
            raise AssertionError(f"Source has more than one owner: {raw}")
        if (path.parts[0] == "NSCharacter") != (owner == "character"):
            raise AssertionError(f"Wrong architectural layer: {raw}")
        listed[raw] = name

actual = set()
for entry in root.iterdir():
    if entry.name == "third_party":
        continue
    candidates = entry.rglob("*") if entry.is_dir() else (entry,)
    actual.update(p.relative_to(root).as_posix() for p in candidates
                  if p.is_file() and p.suffix in (".h", ".cpp"))
missing, stale = actual - listed.keys(), listed.keys() - actual
if missing or stale:
    raise AssertionError(f"Unclassified: {sorted(missing)}; missing sources: {sorted(stale)}")

include = re.compile(r'^\s*#\s*include\s*([<"])([^">]+)[">]', re.MULTILINE)
headers = {(root / path).resolve() for path in actual if path.endswith(".h")}
header_aliases = {}
for raw in sorted(actual):
    path = PurePosixPath(raw)
    if path.suffix != ".h":
        continue
    # The installed API is flat except for the character namespace, regardless
    # of the responsibility directory used by the source tree.
    alias = raw if path.parts[0] == "NSCharacter" else path.name
    if alias in header_aliases:
        raise AssertionError(f"Ambiguous public header spelling: {alias}")
    header_aliases[alias] = (root / raw).resolve()
graph = {path: [] for path in headers}
character_graph = {name: set() for name in ids if owners[name] == "character"}
core_graph = {name: set() for name in ids if owners[name] == "core"}
forbidden_core_edges = {
    ("core_geometry", "core_skeleton_skin"),
    ("core_geometry", "core_runtime"),
    ("core_runtime", "core_geometry"),
    ("core_runtime", "core_authoring"),
    ("core_runtime", "core_rig_animation"),
    ("core_interchange", "core_authoring"),
    ("core_rig_animation", "core_authoring"),
    ("core_rig_animation", "core_interchange"),
}
for raw in sorted(actual):
    source = (root / raw).resolve()
    for delimiter, target in include.findall(source.read_text(encoding="utf-8")):
        if target == "iiActorObject.h" and not raw.startswith("NSCharacter/"):
            raise AssertionError(f"Core source includes the upper-layer umbrella: {raw}")
        if target.startswith("iiActorObject/"):
            alias = target[len("iiActorObject/"):]
            resolved = header_aliases.get(alias, (root / alias).resolve())
        elif delimiter == '"':
            resolved = (source.parent / target).resolve()
            if resolved not in headers:
                resolved = header_aliases.get(target, resolved)
        else:
            continue
        try:
            rel = resolved.relative_to(root)
        except ValueError:
            continue
        if not raw.startswith("NSCharacter/") and rel.parts and rel.parts[0] == "NSCharacter":
            raise AssertionError(f"Core source includes character layer: {raw} -> {rel}")
        other = listed.get(rel.as_posix())
        group = listed[raw]
        if other and group in ("core_mesh_mask_data", "core_edit_layer_data", "core_project_options") and other != group:
            raise AssertionError(f"Project value data depends on another responsibility: {raw} -> {rel}")
        if other and group == "core_pose_program_data" and other not in (group, "core_utility"):
            raise AssertionError(f"Pose program data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_template_types" and other not in (group, "core_mesh_foundation"):
            raise AssertionError(f"Template data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_template_morph_data" and other not in (group, "core_template_types"):
            raise AssertionError(f"Template morph data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_adjustment_data" and other not in (group, "core_asset_data"):
            raise AssertionError(f"Adjustment data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_project_archive" and other not in (group, "core_adjustment_data", "core_skeletal_motion", "core_mesh_mask_data", "core_edit_layer_data", "core_progressive_data", "core_pose_program_data", "core_template_morph_data", "core_template_types", "core_geometry"):
            raise AssertionError(f"Project archive depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_project_version" and other not in (group, "core_project_archive"):
            raise AssertionError(f"Project version selection depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_project_codec" and other not in (group, "core_project_archive", "core_project_options", "core_project_version", "core_skeletal_motion"):
            raise AssertionError(f"Project codec depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_project_adjustment_validation" and other not in (group, "core_adjustment_data", "core_utility"):
            raise AssertionError(f"Project adjustment validation depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_project_rig_archive" and other not in (group, "core_project_archive", "core_utility"):
            raise AssertionError(f"Project rig archive depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_project_history_selection" and other not in (group, "core_point_selection_data", "core_point_selection_validation", "core_asset_data"):
            raise AssertionError(f"Project history selection depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_external_identity" and other not in (group, "core_utility"):
            raise AssertionError(f"External identity data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_external_protocol" and other != group:
            raise AssertionError(f"External identity protocol depends on another responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_external_identity" and other not in (group, "core_external_identity", "core_external_protocol", "core_fbx_metadata", "core_mesh_foundation"):
            raise AssertionError(f"FBX external identity reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_scene_metadata" and other not in (group, "core_fbx_metadata", "core_asset_data", "core_skeletal_motion"):
            raise AssertionError(f"FBX scene metadata reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_external_identity_write" and other not in (group, "core_external_identity", "core_external_protocol", "core_fbx_binary", "core_fbx_scene_nodes", "core_mesh_data"):
            raise AssertionError(f"FBX external identity writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_scene_metadata_write" and other not in (group, "core_fbx_binary", "core_fbx_scene_nodes"):
            raise AssertionError(f"FBX scene metadata writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_progressive_plan_data" and other not in (group, "core_progressive_data"):
            raise AssertionError(f"FBX progressive plan data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_morph_channels" and other not in (group, "core_mesh_data", "core_utility"):
            raise AssertionError(f"Logical morph channels depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_morph_channels_write" and other not in (group, "core_fbx_animation_nodes", "core_fbx_binary", "core_fbx_progressive_plan_data", "core_mesh_data", "core_morph_channels", "core_fbx_morph_write", "core_fbx_scene_nodes"):
            raise AssertionError(f"FBX morph channel writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_shared_geometry" and other not in (group, "core_fbx_binary", "core_mesh_data"):
            raise AssertionError(f"FBX shared geometry comparison depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_geometry_registry" and other not in (group, "core_fbx_binary", "core_fbx_shared_geometry", "core_mesh_data"):
            raise AssertionError(f"FBX geometry registry depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_utility" and other != group and owners[other] == "core":
            raise AssertionError(f"Core utility depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_binary" and other != group:
            raise AssertionError(f"FBX binary writer depends on another responsibility: {raw} -> {rel}")
        if other and group == "core_glb_buffer" and other not in (group, "core_utility"):
            raise AssertionError(f"GLB buffer writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_material_data" and other != group:
            raise AssertionError(f"GLB material data depends on another responsibility: {raw} -> {rel}")
        if other and group == "core_glb_material_plan" and other not in (group, "core_glb_material_data", "core_mesh_data", "core_material_data"):
            raise AssertionError(f"GLB material plan depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_material_write" and other not in (group, "core_glb_material_data", "core_glb_buffer", "core_asset_data", "core_material_data"):
            raise AssertionError(f"GLB material writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_scene_write" and other not in (group, "core_glb_buffer", "core_asset_data"):
            raise AssertionError(f"GLB scene writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_mesh_tangents" and other not in (group, "core_mesh_foundation", "core_utility"):
            raise AssertionError(f"Mesh tangent computation depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_tangent_data" and other not in (group, "core_mesh_tangents"):
            raise AssertionError(f"GLB tangent data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_tangent_plan" and other not in (group, "core_glb_tangent_data", "core_mesh_data", "core_utility"):
            raise AssertionError(f"GLB tangent plan depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_point_attribute_write" and other not in (group, "core_glb_buffer", "core_mesh_data"):
            raise AssertionError(f"GLB point attribute writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_tangent_write" and other not in (group, "core_glb_tangent_data", "core_glb_point_attribute_write", "core_glb_buffer", "core_mesh_data"):
            raise AssertionError(f"GLB tangent writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_shared_mesh_write" and other not in (group, "core_glb_point_attribute_write", "core_glb_buffer", "core_mesh_data"):
            raise AssertionError(f"GLB shared mesh writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_mesh_extras_write" and other not in (group, "core_glb_point_identity_codec", "core_glb_buffer", "core_mesh_data", "core_skeletal_motion"):
            raise AssertionError(f"GLB mesh extras writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_primitive_plan" and other not in (group, "core_mesh_data", "core_material_data"):
            raise AssertionError(f"GLB primitive planner depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_tangent_projection" and other not in (group, "core_mesh_data"):
            raise AssertionError(f"FBX tangent projection depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_fbx_tangent_route" and other not in (group, "core_fbx_tangent_projection", "core_mesh_data"):
            raise AssertionError(f"GLB FBX tangent route depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_mesh_write" and other not in (group, "core_glb_buffer", "core_glb_fbx_tangent_route", "core_glb_material_plan", "core_glb_primitive_plan", "core_glb_tangent_plan", "core_glb_tangent_write", "core_glb_shared_mesh_write", "core_glb_mesh_extras_write", "core_material_data", "core_mesh_data", "core_skeletal_motion"):
            raise AssertionError(f"GLB mesh writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_skin_write" and other not in (group, "core_glb_buffer", "core_utility"):
            raise AssertionError(f"GLB skin writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_container_write" and other not in (group, "core_glb_buffer"):
            raise AssertionError(f"GLB container writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_document_write" and other not in (group, "core_glb_material_data", "core_glb_buffer", "core_glb_material_write", "core_glb_scene_write", "core_asset_data", "core_material_data", "core_mesh_data"):
            raise AssertionError(f"GLB document writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_animation_options" and other not in (group, "core_skeletal_motion"):
            raise AssertionError(f"GLB animation options depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_animation_write" and other not in (group, "core_glb_animation_options", "core_glb_buffer", "core_asset_data"):
            raise AssertionError(f"GLB animation writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_scene_nodes" and other not in (group, "core_fbx_binary", "core_utility"):
            raise AssertionError(f"FBX scene nodes depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_pose_write" and other not in (group, "core_fbx_binary", "core_fbx_scene_nodes", "core_utility"):
            raise AssertionError(f"FBX pose writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_mesh_scene_write" and other not in (group, "core_fbx_binary", "core_fbx_scene_nodes", "core_fbx_pose_write", "core_utility"):
            raise AssertionError(f"FBX mesh scene writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_animation_nodes" and other not in (group, "core_fbx_scene_nodes", "core_fbx_binary", "core_utility"):
            raise AssertionError(f"FBX animation nodes depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_tangent_data" and other not in (group, "core_utility"):
            raise AssertionError(f"FBX tangent data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_tangent_read" and other not in (group, "core_utility", "core_mesh_foundation", "core_fbx_metadata"):
            raise AssertionError(f"FBX tangent reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group in ("core_progressive_data", "core_fbx_animation_data") and other != group:
            raise AssertionError(f"Morph or animation data depends on another responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_progressive_metadata" and other not in (group, "core_fbx_metadata"):
            raise AssertionError(f"FBX progressive metadata depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_morph_channel" and other not in (group, "core_fbx_animation_data", "core_progressive_data", "core_fbx_morph_shape", "core_fbx_progressive_metadata", "core_fbx_metadata", "core_mesh_data", "core_utility"):
            raise AssertionError(f"FBX morph channels depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_geometry_write" and other not in (group, "core_fbx_tangent_data", "core_fbx_binary", "core_fbx_scene_nodes", "core_fbx_topology", "core_mesh_data"):
            raise AssertionError(f"FBX geometry writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_geometry_read" and other not in (group, "core_utility", "core_mesh_data", "core_fbx_skin", "core_fbx_topology", "core_fbx_metadata"):
            raise AssertionError(f"FBX geometry reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_mesh_scene_read" and other not in (group, "core_fbx_geometry_read", "core_fbx_metadata", "core_external_identity", "core_fbx_external_identity", "core_fbx_morph_channel", "core_fbx_tangent_read"):
            raise AssertionError(f"FBX mesh scene reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_animation_stage" and other not in (group, "core_skeletal_motion", "core_mesh_data"):
            raise AssertionError(f"FBX animation stage depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_mesh_data" and other not in (group, "core_mesh_foundation", "core_utility"):
            raise AssertionError(f"Mesh data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_flat_normals" and other not in (group, "core_mesh_foundation", "core_utility"):
            raise AssertionError(f"GLB flat normals depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_topology" and other not in (group, "core_mesh_data", "core_mesh_foundation", "core_geometry"):
            raise AssertionError(f"Template surface topology depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_meshes" and other not in (group, "core_mesh_data"):
            raise AssertionError(f"Template surface mesh assembly depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_material_routing" and other not in (group, "core_mesh_data"):
            raise AssertionError(f"Template surface material routing depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_material_native" and other not in (group, "core_surface_material_routing", "core_surface_resources"):
            raise AssertionError(f"Template surface material adapter depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_resources" and other not in (group, "core_asset_data"):
            raise AssertionError(f"Template surface resources depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_replacement_resources" and other not in (group, "core_asset_data", "core_surface_resources"):
            raise AssertionError(f"Template surface replacement resource planning depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_replacement_layers" and other not in (group, "core_adjustment_data", "core_surface_replacement_resources", "core_surface_resources"):
            raise AssertionError(f"Template surface replacement layer processing depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_slots" and other not in (group, "core_skeletal_motion"):
            raise AssertionError(f"Template surface slot planning depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_edit_remap" and other not in (group, "core_mesh_mask_data", "core_adjustment_data"):
            raise AssertionError(f"Template surface edit remapping depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_point_selection_data" and other not in (group, "core_mesh_mask_data"):
            raise AssertionError(f"Point selection data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_mask_validation" and other not in (group, "core_mesh_mask_data", "core_asset_data"):
            raise AssertionError(f"Point mask validation depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_point_selection_validation" and other not in (group, "core_point_selection_data", "core_asset_data", "core_mask_validation", "core_utility"):
            raise AssertionError(f"Point selection validation depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_selection_remap" and other not in (group, "core_point_selection_data", "core_surface_edit_remap"):
            raise AssertionError(f"Template surface selection remapping depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_references" and other not in (group, "core_template_types", "core_geometry", "core_mesh_data", "core_surface_topology"):
            raise AssertionError(f"Template surface references depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_followers" and other not in (group, "core_asset_data", "core_geometry", "core_surface_references"):
            raise AssertionError(f"Template surface followers depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_surface_follower_native" and other not in (group, "core_surface_followers", "core_skeleton_skin"):
            raise AssertionError(f"Template surface follower adapter depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_point_identity_codec" and other not in (group, "core_utility", "core_mesh_data", "core_skeletal_motion"):
            raise AssertionError(f"GLB point identity codec depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_point_restore_core" and other not in (group, "core_glb_point_identity_codec", "core_mesh_foundation", "core_utility"):
            raise AssertionError(f"GLB point restoration depends on a higher responsibility: {raw} -> {rel}")
        if other and group in ("core_glb_document", "core_glb_image_api") and other != group:
            raise AssertionError(f"GLB document or image interface depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_import_options" and other not in (group, "core_glb_document"):
            raise AssertionError(f"GLB import options depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_json" and other not in (group, "core_glb_import_options"):
            raise AssertionError(f"GLB JSON fields depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_accessor_read" and other not in (group, "core_glb_document", "core_glb_json"):
            raise AssertionError(f"GLB accessor reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_geometry_budget" and other != group:
            raise AssertionError(f"GLB geometry budget depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_transform" and other not in (group, "core_glb_json", "core_utility"):
            raise AssertionError(f"GLB transform depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_mesh_bundle" and other not in (group, "core_glb_bundle_data", "core_glb_geometry_budget", "core_glb_mesh_attributes", "core_glb_accessor_read", "core_glb_import_options", "core_glb_transform", "core_mesh_foundation", "core_utility"):
            raise AssertionError(f"GLB mesh bundle depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_bundle_data" and other != group:
            raise AssertionError(f"GLB bundle data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_bundle_index" and other not in (group, "core_glb_bundle_data"):
            raise AssertionError(f"GLB bundle index depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_primitive_topology" and other not in (group, "core_mesh_foundation"):
            raise AssertionError(f"GLB primitive topology depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_tangent_notes" and other not in (group, "core_glb_json"):
            raise AssertionError(f"GLB tangent notes depend on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_tangent_snapshot" and other not in (group, "core_mesh_foundation"):
            raise AssertionError(f"GLB tangent snapshot depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_glb_mesh_attributes" and other not in (group, "core_glb_json", "core_glb_import_options", "core_utility"):
            raise AssertionError(f"GLB mesh attribute parser depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_material_data" and other != group:
            raise AssertionError(f"Material data depends on another responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_resource_write" and other not in (group, "core_fbx_scene_nodes", "core_fbx_binary", "core_asset_data", "core_material_data"):
            raise AssertionError(f"FBX resource writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_metadata" and other != group:
            raise AssertionError(f"FBX metadata decoder depends on another responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_image_api" and other != group:
            raise AssertionError(f"FBX image interface depends on another responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_resources" and other not in (group, "core_fbx_image_api", "core_fbx_metadata", "core_asset_data"):
            raise AssertionError(f"FBX resource reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_skeleton" and other not in (group, "core_utility", "core_fbx_metadata", "core_asset_data"):
            raise AssertionError(f"FBX skeleton reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_skin" and other not in (group, "core_utility", "core_fbx_metadata"):
            raise AssertionError(f"FBX skin reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_skin_data" and other not in (group, "core_utility"):
            raise AssertionError(f"FBX skin data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_skeleton_write" and other not in (group, "core_skeletal_motion", "core_fbx_skin_data", "core_fbx_binary", "core_fbx_scene_nodes", "core_fbx_pose_write", "core_fbx_scene_metadata_write"):
            raise AssertionError(f"FBX skeleton writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_skin_write" and other not in (group, "core_utility", "core_mesh_foundation", "core_fbx_skin_data", "core_fbx_binary", "core_fbx_scene_nodes"):
            raise AssertionError(f"FBX skin writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_morph_shape" and other not in (group, "core_utility", "core_mesh_foundation", "core_fbx_metadata"):
            raise AssertionError(f"FBX morph shape reader depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_morph_write" and other not in (group, "core_utility", "core_mesh_foundation", "core_fbx_binary", "core_fbx_scene_nodes"):
            raise AssertionError(f"FBX morph writer depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_fbx_topology" and other not in (group, "core_utility", "core_mesh_foundation", "core_fbx_metadata", "core_fbx_binary"):
            raise AssertionError(f"FBX topology metadata depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_skeletal_motion" and other not in (group, "core_utility"):
            raise AssertionError(f"Skeletal motion depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_mesh_foundation" and other not in (group, "core_utility"):
            raise AssertionError(f"Mesh foundation depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_asset_data" and other not in (group, "core_utility", "core_mesh_foundation", "core_mesh_data", "core_skeletal_motion", "core_material_data"):
            raise AssertionError(f"Asset data depends on a higher responsibility: {raw} -> {rel}")
        if other and group == "core_deformation" and other not in (group, "core_utility", "core_mesh_foundation", "core_skeletal_motion"):
            raise AssertionError(f"Deformation depends on a higher responsibility: {raw} -> {rel}")
        if other and (group, other) in forbidden_core_edges:
            raise AssertionError(f"Core responsibility boundary: {raw} -> {rel}")
        if (other == "core_composition" and group != other and owners[group] == "core"):
            raise AssertionError(f"Core source includes a compatibility aggregate: {raw} -> {rel}")
        if other and group != other and owners[group] == owners[other] == "character":
            character_graph[group].add(other)
        if other and group != other and owners[group] == owners[other] == "core":
            core_graph[group].add(other)
        if source in graph and resolved in graph:
            graph[source].append(resolved)

state, stack = {}, []


def visit(path):
    state[path] = 1
    stack.append(path)
    for dependency in graph[path]:
        if state.get(dependency) == 1:
            cycle = stack[stack.index(dependency):] + [dependency]
            raise AssertionError("Header cycle: " + " -> ".join(str(p.relative_to(root)) for p in cycle))
        if not state.get(dependency):
            visit(dependency)
    stack.pop()
    state[path] = 2


for path in sorted(headers):
    if not state.get(path):
        visit(path)

# Focused APIs must remain independent of unrelated higher-level interfaces.
# Compatibility aggregates deliberately retain their original wider includes.
focused_boundaries = {
    "MeshTangents.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbTangentData.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbTangentPlan.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbPointAttributeWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbTangentWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbTangentPlan.h"),
    "Object3DGlbSharedMeshWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbMeshExtrasWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbPrimitivePlan.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DFbxTangentProjection.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbFbxTangentRoute.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbMeshWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbSkinWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbContainerWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbDocumentWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbMaterialVariant.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbMaterialPlan.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbMaterialWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbSceneWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbTangentSnapshot.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbMesh.h"),
    "Object3DGlbBundleData.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbMesh.h"),
    "Object3DGlbBundleIndex.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbMesh.h"),
    "Object3DGlbTangentNotes.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbMesh.h"),
    "Object3DGlbPrimitiveTopology.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbMesh.h"),
    "Object3DGlbMeshBundle.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbMesh.h", "Object3DGlbSkin.h"),
    "Object3DGlbTransform.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbScene.h"),
    "Object3DGlbGeometryBudget.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbMesh.h"),
    "Object3DGlbAccessorRead.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbMesh.h"),
    "GlbDocument.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "GlbImageResolver.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbImportOptions.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbPointRestoreCore.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h", "Object3DGlbMesh.h"),
    "Object3DGlbPointIdentity.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DTemplateSurfaceResources.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DAdjustment.h"),
    "Object3DTemplateSurfaceSlots.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h"),
    "Object3DTemplateSurfaceMeshes.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h", "Object3DProject.h"),
    "Object3DTemplateSurfaceMaterialRouting.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h", "Object3DProject.h"),
    "Object3DTemplateSurfaceMaterialNative.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h", "Object3DProject.h"),
    "Object3DTemplateSurfaceEditRemap.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h"),
    "MeshPointMaskValidation.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h"),
    "Object3DSelectionData.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h"),
    "Object3DSelectionState.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h"),
    "Object3DSelectionValidation.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h"),
    "Object3DTemplateSurfaceSelectionRemap.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h"),
    "Object3DTemplateSurfaceReferences.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h", "Object3DProject.h"),
    "Object3DTemplateSurfaceReplacementResources.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h", "Object3DProject.h"),
    "Object3DTemplateSurfaceReplacementLayers.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h", "Object3DProject.h"),
    "Object3DTemplateSurfaceFollowers.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h", "Object3DProject.h"),
    "Object3DTemplateSurfaceFollowerNative.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DTemplate.h", "Object3DProject.h"),
    "Object3DGlbAnimationOptions.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DGlbAnimationWrite.h": ("Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DProjectCodec.h": ("Object3DProject.h", "Object3DProjectState.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DProjectVersion.h": ("Object3DProject.h", "Object3DProjectState.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DProjectAdjustmentValidation.h": ("Object3DProject.h", "Object3DProjectState.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DAsset.h"),
    "Object3DProjectRigArchive.h": ("Object3DProject.h", "Object3DProjectState.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DAsset.h"),
    "Object3DProjectRigRestore.h": ("Object3DProject.h", "Object3DProjectState.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DAsset.h"),
    "Object3DExternalIdentity.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DExternalProtocol.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxExternalIdentityRead.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxSceneMetadata.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxExternalIdentityWrite.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxSceneMetadataWrite.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxSkinData.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "MeshTopologyCore.h"),
    "Object3DFbxPoseWrite.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxMeshSceneWrite.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxGeometryRegistry.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxSkeletonWrite.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxProgressiveData.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DRig.h", "Object3DProgressiveMorphCore.h"),
    "Object3DMorphChannels.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DFbxMorphChannelsWrite.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DFbxSharedGeometry.h": ("Object3DData.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DInterchange.h"),
    "Object3DProjectArchive.h": ("Object3DProject.h", "Object3DProjectState.h", "Object3DEngine.h", "Object3DEditor.h", "Object3DAsset.h", "Object3DTemplate.h", "Object3DRig.h"),
    "MeshPointMask.h": ("Object3DEngine.h", "Object3DEditor.h"),
    "Object3DEditLayerData.h": ("Object3DEngine.h", "Object3DEditor.h"),
    "Object3DPoseProgramData.h": ("Object3DEngine.h", "Object3DEditor.h"),
    "Object3DProjectOptions.h": ("Object3DEngine.h", "Object3DEditor.h"),
    "Object3DTemplateTypes.h": ("Object3DEngine.h", "Object3DEditor.h"),
    "Object3DTemplateMorphData.h": ("Object3DEngine.h", "Object3DEditor.h"),
    "SurfaceFitSettings.h": ("Object3DEngine.h", "Object3DEditor.h"),
    "Object3DMesh.h": ("Object3DData.h", "Object3DAsset.h", "Skeleton.h", "PoseReference.h", "RigSampleCatalogCore.h", "Object3DEngine.h"),
    "Object3DMaterial.h": ("Object3DData.h", "Object3DAsset.h", "MeshTopologyCore.h", "Skeleton.h", "Object3DEngine.h"),
    "Object3DProgressiveMorphCore.h": ("Object3DProject.h", "Object3DInterchange.h", "Object3DEditor.h"),
    "MeshTopologyCore.h": ("SkeletalMesh.h", "Skeleton.h", "SceneComponent.h"),
    "MeshGeometryCore.h": ("SkeletalMesh.h", "Skeleton.h", "SceneComponent.h"),
    "SurfaceBindingCore.h": ("Object3DAsset.h", "SkinBinding.h", "SkeletalMesh.h", "Skeleton.h", "SceneComponent.h"),
    "Object3DInstanceTransferCore.h": ("Object3DEditor.h", "Object3DEditorRig.h"),
    "RigSampleCatalogCore.h": ("RigSampleCatalogExport.h", "Object3DAsset.h", "Object3DEngine.h", "Object3DEditor.h"),
    "Object3DData.h": ("Object3DAsset.h", "Character.h", "SkeletalMesh.h", "SceneComponent.h", "Object3DEngine.h"),
    "SkeletalMeshCore.h": ("SkeletalMeshComponent.h", "SceneComponent.h", "Character.h", "Object3DAsset.h", "Object3DEngine.h"),
    "MeshScreenSelectionCore.h": ("Object3DAsset.h", "Character.h", "SceneComponent.h", "Object3DEngine.h"),
    "MeshMaskProjectionCore.h": ("Object3DAsset.h", "Character.h", "SceneComponent.h", "Object3DEngine.h"),
}
for focused, forbidden in focused_boundaries.items():
    focused_path = header_aliases.get(focused)
    if focused_path not in graph:
        raise AssertionError(f"Focused header missing: {focused}")
    excluded = {header_aliases[name] for name in forbidden if name in header_aliases}
    seen, pending = set(), [focused_path]
    while pending:
        path = pending.pop()
        if path in seen:
            continue
        if path in excluded:
            raise AssertionError(f"Focused {focused} includes higher responsibility: {path.relative_to(root)}")
        seen.add(path)
        pending.extend(graph.get(path, ()))

state, stack = {}, []


def visit_character(group):
    state[group] = 1
    stack.append(group)
    for dependency in character_graph[group]:
        if state.get(dependency) == 1:
            raise AssertionError("Character responsibility cycle: " +
                                 " -> ".join(stack[stack.index(dependency):] + [dependency]))
        if not state.get(dependency):
            visit_character(dependency)
    stack.pop()
    state[group] = 2


for group in sorted(character_graph):
    if not state.get(group):
        visit_character(group)

# Reject cycles between responsibilities as well as header-level cycles.
# This graph reflects direct textual includes, not all link/runtime dependencies.
counter, indices, low, active, on_stack, components = 0, {}, {}, [], set(), []


def visit_core(group):
    global counter
    indices[group] = low[group] = counter
    counter += 1
    active.append(group)
    on_stack.add(group)
    for dependency in core_graph[group]:
        if dependency not in indices:
            visit_core(dependency)
            low[group] = min(low[group], low[dependency])
        elif dependency in on_stack:
            low[group] = min(low[group], indices[dependency])
    if low[group] == indices[group]:
        component = []
        while True:
            member = active.pop()
            on_stack.remove(member)
            component.append(member)
            if member == group:
                break
        if len(component) > 1:
            components.append(sorted(component))


for group in sorted(core_graph):
    if group not in indices:
        visit_core(group)
if components:
    raise AssertionError("Core responsibility group cycle: " + "; ".join(", ".join(group) for group in sorted(components)))
print(f"Classified {len(actual)} source files in {len(modules)} groups; protected boundaries and direct include-group cycles passed")
