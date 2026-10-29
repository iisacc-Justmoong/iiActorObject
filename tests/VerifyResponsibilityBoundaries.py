"""Exercise both allowed and forbidden dependency shapes on small source fixtures."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

project = Path(sys.argv[1]).resolve()
build = Path(sys.argv[2]).resolve()
checker = project / "tests/VerifyResponsibilityMap.py"

with tempfile.TemporaryDirectory(prefix="responsibility-boundaries-", dir=build) as directory:
    root = Path(directory)
    source = root / "src"
    (source / "NSCharacter").mkdir(parents=True)
    manifest_path = root / "resources/architecture/ResponsibilityMap.json"
    manifest_path.parent.mkdir(parents=True)
    modules = [
        ("core_utility", "core", {"Transform.h": "#pragma once\n", "MeshData.h": "#pragma once\n"}),
        ("core_fbx_binary", "core", {"FbxBinary.h": "#pragma once\n"}),
        ("core_glb_buffer", "core", {"GlbBuffer.h": '#include "MeshData.h"\n'}),
        ("core_glb_material_data", "core", {"Object3DGlbMaterialVariant.h": "#pragma once\n"}),
        ("core_glb_material_plan", "core", {"Object3DGlbMaterialPlan.h": '#include "Object3DGlbMaterialVariant.h"\n#include "Object3DMaterial.h"\n#include "Object3DMesh.h"\n'}),
        ("core_glb_material_write", "core", {"Object3DGlbMaterialWrite.h": '#include "Object3DGlbMaterialVariant.h"\n#include "GlbBuffer.h"\n#include "Object3DData.h"\n'}),
        ("core_glb_scene_write", "core", {"Object3DGlbSceneWrite.h": '#include "GlbBuffer.h"\n#include "Object3DData.h"\n'}),
        ("core_mesh_tangents", "core", {"MeshTangents.h": '#include "MeshTopologyCore.h"\n'}),
        ("core_glb_tangent_data", "core", {"Object3DGlbTangentData.h": '#include "MeshTangents.h"\n'}),
        ("core_glb_tangent_plan", "core", {"Object3DGlbTangentPlan.h": '#include "Object3DGlbTangentData.h"\n#include "Object3DMesh.h"\n'}),
        ("core_glb_point_attribute_write", "core", {"Object3DGlbPointAttributeWrite.h": '#include "GlbBuffer.h"\n#include "Object3DMesh.h"\n'}),
        ("core_glb_tangent_write", "core", {"Object3DGlbTangentWrite.h": '#include "Object3DGlbTangentData.h"\n#include "Object3DGlbPointAttributeWrite.h"\n'}),
        ("core_glb_shared_mesh_write", "core", {"Object3DGlbSharedMeshWrite.h": '#include "GlbBuffer.h"\n#include "Object3DGlbPointAttributeWrite.h"\n#include "Object3DMesh.h"\n'}),
        ("core_glb_mesh_extras_write", "core", {"Object3DGlbMeshExtrasWrite.h": '#include "GlbBuffer.h"\n#include "Object3DGlbPointIdentity.h"\n#include "Object3DMesh.h"\n'}),
        ("core_glb_primitive_plan", "core", {"Object3DGlbPrimitivePlan.h": '#include "Object3DMaterial.h"\n#include "Object3DMesh.h"\n'}),
        ("core_fbx_tangent_projection", "core", {"Object3DFbxTangentProjection.h": '#include "Object3DMesh.h"\n'}),
        ("core_glb_fbx_tangent_route", "core", {"Object3DGlbFbxTangentRoute.h": '#include "Object3DFbxTangentProjection.h"\n#include "Object3DMesh.h"\n'}),
        ("core_glb_mesh_write", "core", {"Object3DGlbMeshWrite.h": '#include "Object3DGlbPrimitivePlan.h"\n#include "Object3DGlbFbxTangentRoute.h"\n#include "Object3DGlbMaterialPlan.h"\n'}),
        ("core_glb_skin_write", "core", {"Object3DGlbSkinWrite.h": '#include "GlbBuffer.h"\n#include "Transform.h"\n'}),
        ("core_glb_container_write", "core", {"Object3DGlbContainerWrite.h": '#include "GlbBuffer.h"\n'}),
        ("core_glb_document_write", "core", {"Object3DGlbDocumentWrite.h": '#include "GlbBuffer.h"\n#include "Object3DGlbSceneWrite.h"\n#include "Object3DGlbMaterialWrite.h"\n'}),
        ("core_glb_animation_options", "core", {"Object3DGlbAnimationOptions.h": '#include "Motion.h"\n'}),
        ("core_glb_animation_write", "core", {"Object3DGlbAnimationWrite.h": '#include "Object3DGlbAnimationOptions.h"\n#include "GlbBuffer.h"\n#include "Object3DData.h"\n'}),
        ("core_fbx_scene_nodes", "core", {"FbxSceneNodes.h": '#include "FbxBinary.h"\n#include "Transform.h"\n'}),
        ("core_fbx_pose_write", "core", {"Object3DFbxPoseWrite.h": '#include "FbxBinary.h"\n#include "Transform.h"\n'}),
        ("core_fbx_mesh_scene_write", "core", {"Object3DFbxMeshSceneWrite.h": '#include "FbxBinary.h"\n#include "Object3DFbxPoseWrite.h"\n'}),
        ("core_fbx_animation_nodes", "core", {"FbxAnimationNodes.h": '#include "FbxSceneNodes.h"\n#include "FbxBinary.h"\n#include "Transform.h"\n'}),
        ("core_fbx_tangent_data", "core", {"FbxTangentData.h": '#include "Transform.h"\n'}),
        ("core_fbx_tangent_read", "core", {"FbxTangentRead.h": '#include "FbxMetadata.h"\n#include "MeshTopologyCore.h"\n'}),
        ("core_progressive_data", "core", {"ProgressiveMorphData.h": "#pragma once\n"}),
        ("core_fbx_animation_data", "core", {"FbxAnimationData.h": "#pragma once\n"}),
        ("core_fbx_progressive_metadata", "core", {"FbxProgressiveMetadata.h": '#include "FbxMetadata.h"\n'}),
        ("core_fbx_morph_channel", "core", {"FbxMorphChannelRead.h": '#include "FbxAnimationData.h"\n#include "ProgressiveMorphData.h"\n#include "FbxProgressiveMetadata.h"\n#include "Object3DMesh.h"\n'}),
        ("core_fbx_geometry_write", "core", {"FbxGeometryWrite.h": '#include "FbxBinary.h"\n#include "FbxSceneNodes.h"\n#include "FbxTangentData.h"\n#include "Object3DMesh.h"\n'}),
        ("core_fbx_geometry_read", "core", {"FbxGeometryRead.h": '#include "Object3DMesh.h"\n#include "FbxSkin.h"\n#include "FbxTopology.h"\n'}),
        ("core_fbx_mesh_scene_read", "core", {"Object3DFbxMeshSceneRead.h": '#include "FbxGeometryRead.h"\n#include "Object3DExternalIdentity.h"\n'}),
        ("core_fbx_animation_stage", "core", {"Object3DFbxAnimationStage.h": '#include "Motion.h"\n#include "Object3DMesh.h"\n'}),
        ("core_material_data", "core", {"Object3DMaterial.h": "#pragma once\n"}),
        ("core_fbx_resource_write", "core", {"FbxResourceWrite.h": '#include "FbxSceneNodes.h"\n#include "FbxBinary.h"\n#include "Object3DMaterial.h"\n'}),
        ("core_fbx_metadata", "core", {"FbxMetadata.h": "#pragma once\n"}),
        ("core_external_identity", "core", {"Object3DExternalIdentity.h": "#pragma once\n"}),
        ("core_external_protocol", "core", {"Object3DExternalProtocol.h": "#pragma once\n"}),
        ("core_fbx_external_identity", "core", {"Object3DFbxExternalIdentityRead.h": '#include "Object3DExternalIdentity.h"\n'}),
        ("core_fbx_scene_metadata", "core", {"Object3DFbxSceneMetadata.h": '#include "RigSampleCatalogCore.h"\n'}),
        ("core_fbx_external_identity_write", "core", {"Object3DFbxExternalIdentityWrite.h": '#include "FbxBinary.h"\n'}),
        ("core_fbx_scene_metadata_write", "core", {"Object3DFbxSceneMetadataWrite.h": '#include "FbxBinary.h"\n'}),
        ("core_fbx_image_api", "core", {"FbxImageResolver.h": "#pragma once\n"}),
        ("core_fbx_resources", "core", {"FbxResources.h": '#include "FbxImageResolver.h"\n#include "FbxMetadata.h"\n#include "Object3DData.h"\n'}),
        ("core_fbx_skeleton", "core", {"FbxSkeleton.h": '#include "FbxMetadata.h"\n#include "Object3DData.h"\n'}),
        ("core_fbx_skin", "core", {"FbxSkin.h": '#include "FbxMetadata.h"\n#include "MeshData.h"\n'}),
        ("core_fbx_skin_data", "core", {"Object3DFbxSkinData.h": '#include "Transform.h"\n'}),
        ("core_fbx_skeleton_write", "core", {"Object3DFbxSkeletonWrite.h": '#include "Object3DFbxSkinData.h"\n#include "Object3DFbxPoseWrite.h"\n#include "FbxBinary.h"\n'}),
        ("core_fbx_skin_write", "core", {"FbxSkinWrite.h": '#include "FbxBinary.h"\n#include "FbxSceneNodes.h"\n#include "Object3DFbxSkinData.h"\n#include "MeshData.h"\n'}),
        ("core_fbx_morph_shape", "core", {"FbxMorphShape.h": '#include "FbxMetadata.h"\n#include "MeshTopologyCore.h"\n'}),
        ("core_fbx_morph_write", "core", {"FbxMorphWrite.h": '#include "FbxBinary.h"\n#include "FbxSceneNodes.h"\n#include "MeshTopologyCore.h"\n'}),
        ("core_fbx_progressive_plan_data", "core", {"Object3DFbxProgressiveData.h": '#include "ProgressiveMorphData.h"\n'}),
        ("core_morph_channels", "core", {"Object3DMorphChannels.h": '#include "Object3DMesh.h"\n'}),
        ("core_fbx_morph_channels_write", "core", {"Object3DFbxMorphChannelsWrite.h": '#include "Object3DFbxProgressiveData.h"\n#include "Object3DMesh.h"\n#include "FbxMorphWrite.h"\n'}),
        ("core_fbx_shared_geometry", "core", {"Object3DFbxSharedGeometry.h": '#include "Object3DMesh.h"\n#include "FbxBinary.h"\n'}),
        ("core_fbx_geometry_registry", "core", {"Object3DFbxGeometryRegistry.h": '#include "Object3DMesh.h"\n#include "FbxBinary.h"\n'}),
        ("core_fbx_topology", "core", {"FbxTopology.h": '#include "FbxBinary.h"\n#include "FbxMetadata.h"\n#include "MeshData.h"\n'}),
        ("core_skeletal_motion", "core", {
            "Motion.h": '#include "Transform.h"\n',
            "RigSampleCatalogCore.h": '#include "Motion.h"\n',
        }),
        ("core_runtime", "core", {"Runtime.h": "#pragma once\n", "Object3DInstanceTransferCore.h": "#pragma once\n"}),
        ("core_mesh_foundation", "core", {
            "MeshGeometryCore.h": '#include "MeshData.h"\n',
            "MeshTopologyCore.h": '#include "MeshData.h"\n',
        }),
        ("core_mesh_data", "core", {"Object3DMesh.h": '#include "MeshTopologyCore.h"\n'}),
        ("core_glb_flat_normals", "core", {"GlbFlatNormals.h": '#include "MeshTopologyCore.h"\n#include "Transform.h"\n'}),
        ("core_surface_topology", "core", {"TemplateSurfaceTopology.h": '#include "Object3DMesh.h"\n#include "SurfaceBindingCore.h"\n'}),
        ("core_surface_meshes", "core", {"Object3DTemplateSurfaceMeshes.h": '#include "Object3DMesh.h"\n'}),
        ("core_surface_material_routing", "core", {"Object3DTemplateSurfaceMaterialRouting.h": '#include "Object3DMesh.h"\n'}),
        ("core_surface_material_native", "core", {"Object3DTemplateSurfaceMaterialNative.h": '#include "Object3DTemplateSurfaceMaterialRouting.h"\n'}),
        ("core_surface_resources", "core", {"Object3DTemplateSurfaceResources.h": '#include "Object3DData.h"\n'}),
        ("core_surface_replacement_resources", "core", {"Object3DTemplateSurfaceReplacementResources.h": '#include "Object3DData.h"\n#include "Object3DTemplateSurfaceResources.h"\n'}),
        ("core_surface_replacement_layers", "core", {"Object3DTemplateSurfaceReplacementLayers.h": '#include "Object3DAdjustmentData.h"\n#include "Object3DTemplateSurfaceReplacementResources.h"\n'}),
        ("core_surface_slots", "core", {"Object3DTemplateSurfaceSlots.h": '#include "Motion.h"\n'}),
        ("core_surface_edit_remap", "core", {"Object3DTemplateSurfaceEditRemap.h": '#include "MeshPointMask.h"\n'}),
        ("core_point_selection_data", "core", {"Object3DSelectionData.h": '#include "MeshPointMask.h"\n'}),
        ("core_mask_validation", "core", {"MeshPointMaskValidation.h": '#include "MeshPointMask.h"\n'}),
        ("core_point_selection_validation", "core", {"Object3DSelectionState.h": '#include "Object3DSelectionValidation.h"\n', "Object3DSelectionValidation.h": '#include "Object3DSelectionData.h"\n'}),
        ("core_project_history_selection", "core", {"Object3DProjectHistorySelection.h": '#include "Object3DSelectionData.h"\n'}),
        ("core_surface_selection_remap", "core", {"Object3DTemplateSurfaceSelectionRemap.h": '#include "Object3DSelectionData.h"\n#include "Object3DTemplateSurfaceEditRemap.h"\n'}),
        ("core_surface_references", "core", {"Object3DTemplateSurfaceReferences.h": '#include "Object3DTemplateTypes.h"\n#include "SurfaceBindingCore.h"\n'}),
        ("core_surface_followers", "core", {"Object3DTemplateSurfaceFollowers.h": '#include "Object3DData.h"\n#include "SurfaceBindingCore.h"\n'}),
        ("core_surface_follower_native", "core", {"Object3DTemplateSurfaceFollowerNative.h": '#include "Object3DTemplateSurfaceFollowers.h"\n'}),
        ("core_glb_point_identity_codec", "core", {"Object3DGlbPointIdentity.h": '#include "MeshData.h"\n'}),
        ("core_glb_point_restore_core", "core", {"Object3DGlbPointRestoreCore.h": '#include "Object3DGlbPointIdentity.h"\n#include "MeshTopologyCore.h"\n'}),
        ("core_glb_document", "core", {"GlbDocument.h": "#pragma once\n"}),
        ("core_glb_image_api", "core", {"GlbImageResolver.h": "#pragma once\n"}),
        ("core_glb_import_options", "core", {"Object3DGlbImportOptions.h": '#include "GlbDocument.h"\n'}),
        ("core_glb_json", "core", {"Object3DGlbJson.h": '#include "Object3DGlbImportOptions.h"\n'}),
        ("core_glb_accessor_read", "core", {"Object3DGlbAccessorRead.h": '#include "GlbDocument.h"\n#include "Object3DGlbJson.h"\n'}),
        ("core_glb_geometry_budget", "core", {"Object3DGlbGeometryBudget.h": "#pragma once\n"}),
        ("core_glb_transform", "core", {"Object3DGlbTransform.h": '#include "Object3DGlbJson.h"\n#include "Transform.h"\n'}),
        ("core_glb_mesh_bundle", "core", {"Object3DGlbMeshBundle.h": '#include "Object3DGlbGeometryBudget.h"\n#include "Object3DGlbMeshAttributes.h"\n'}),
        ("core_glb_primitive_topology", "core", {"Object3DGlbPrimitiveTopology.h": '#include "MeshTopologyCore.h"\n'}),
        ("core_glb_tangent_notes", "core", {"Object3DGlbTangentNotes.h": '#include "Object3DGlbJson.h"\n'}),
        ("core_glb_bundle_data", "core", {"Object3DGlbBundleData.h": "#pragma once\n"}),
        ("core_glb_bundle_index", "core", {"Object3DGlbBundleIndex.h": '#include "Object3DGlbBundleData.h"\n'}),
        ("core_glb_tangent_snapshot", "core", {"Object3DGlbTangentSnapshot.h": '#include "MeshTopologyCore.h"\n'}),
        ("core_glb_mesh_attributes", "core", {"Object3DGlbMeshAttributes.h": '#include "Object3DGlbJson.h"\n#include "MeshData.h"\n'}),
        ("core_asset_data", "core", {"Object3DData.h": '#include "MeshTopologyCore.h"\n'}),
        ("core_deformation", "core", {"SkeletalMeshCore.h": '#include "MeshData.h"\n#include "Motion.h"\n'}),
        ("core_geometry", "core", {
            "SurfaceBindingCore.h": '#include "Transform.h"\n',
            "SurfaceFitSettings.h": "#pragma once\n",
            "MeshScreenSelectionCore.h": '#include "Object3DData.h"\n',
            "MeshMaskProjectionCore.h": '#include "Object3DData.h"\n',
        }),
        ("core_skeleton_skin", "core", {"SkeletalMesh.h": "#pragma once\n"}),
        ("core_rig_animation", "core", {
            "Object3DProgressiveMorphCore.h": '#include "Runtime.h"\n',
            "Rig.cpp": '#include "Object3DProgressiveMorphCore.h"\n',
        }),
        ("core_interchange", "core", {"Exchange.h": "#pragma once\n", "Exchange.cpp": '#include "Exchange.h"\n'}),
        ("core_mesh_mask_data", "core", {"MeshPointMask.h": "#pragma once\n"}),
        ("core_edit_layer_data", "core", {"Object3DEditLayerData.h": "#pragma once\n"}),
        ("core_pose_program_data", "core", {"Object3DPoseProgramData.h": '#include "Transform.h"\n'}),
        ("core_project_options", "core", {"Object3DProjectOptions.h": "#pragma once\n"}),
        ("core_template_types", "core", {"Object3DTemplateTypes.h": '#include "MeshTopologyCore.h"\n'}),
        ("core_template_morph_data", "core", {"Object3DTemplateMorphData.h": '#include "Object3DTemplateTypes.h"\n'}),
        ("core_adjustment_data", "core", {"Object3DAdjustmentData.h": '#include "Object3DData.h"\n'}),
        ("core_project_archive", "core", {"Object3DProjectArchive.h": '#include "Object3DAdjustmentData.h"\n#include "SurfaceBindingCore.h"\n'}),
        ("core_project_version", "core", {"Object3DProjectVersion.h": "#pragma once\n"}),
        ("core_project_codec", "core", {"Object3DProjectCodec.h": "#pragma once\n"}),
        ("core_project_adjustment_validation", "core", {"Object3DProjectAdjustmentValidation.h": "#pragma once\n"}),
        ("core_project_rig_archive", "core", {"Object3DProjectRigArchive.h": "#pragma once\n"}),
        ("core_authoring", "core", {"Object3DProject.h": "#pragma once\n", "Object3DEditor.h": "#pragma once\n", "Object3DProjectRigRestore.h": "#pragma once\n"}),
        ("core_composition", "core", {
            "Object3DProgressiveMorph.h": '#include "Object3DProgressiveMorphCore.h"\n#include "Object3DProject.h"\n#include "Exchange.h"\n',
        }),
        ("character_views", "character", {
            "NSCharacter/View.h": '#include <iiActorObject/Object3DProgressiveMorph.h>\n',
        }),
    ]
    manifest = {"schema_version": 1, "source_root": "src", "modules": []}
    for name, owner, files in modules:
        manifest["modules"].append({"id": name, "owner": owner, "responsibility": name, "files": list(files)})
        for filename, content in files.items():
            (source / filename).write_text(content, encoding="utf-8")
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

    def check(expected=None):
        result = subprocess.run([sys.executable, "-B", str(checker), str(root)], capture_output=True, text=True)
        output = result.stdout + result.stderr
        if expected is None and result.returncode:
            raise AssertionError("Allowed dependencies rejected: " + output)
        if expected is not None and (not result.returncode or expected not in output):
            raise AssertionError("Forbidden dependency accepted or misclassified: " + output)

    check()
    # Moving an interface to a responsibility directory must not hide its
    # dependencies from the checker, including forbidden cross-directory edges.
    (source / "Core").mkdir()
    (source / "Transform.h").rename(source / "Core/Transform.h")
    utility = next(module for module in manifest["modules"] if module["id"] == "core_utility")
    utility["files"][utility["files"].index("Transform.h")] = "Core/Transform.h"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    check()
    (source / "Core/Transform.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Core utility depends on a higher responsibility")
    (source / "Core/Transform.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Core/Transform.h").rename(source / "Transform.h")
    utility["files"][utility["files"].index("Core/Transform.h")] = "Transform.h"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    (source / "Object3DProjectHistorySelection.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Project history selection depends on a higher responsibility")
    (source / "Object3DProjectHistorySelection.h").write_text('#include "Object3DSelectionData.h"\n', encoding="utf-8")
    (source / "Object3DFbxMeshSceneRead.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX mesh scene reader depends on a higher responsibility")
    (source / "Object3DFbxMeshSceneRead.h").write_text('#include "FbxGeometryRead.h"\n#include "Object3DExternalIdentity.h"\n', encoding="utf-8")
    (source / "Object3DFbxAnimationStage.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX animation stage depends on a higher responsibility")
    (source / "Object3DFbxAnimationStage.h").write_text('#include "Motion.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DFbxPoseWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX pose writer depends on a higher responsibility")
    (source / "Object3DFbxPoseWrite.h").write_text('#include "FbxBinary.h"\n#include "Transform.h"\n', encoding="utf-8")
    (source / "Object3DFbxMeshSceneWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX mesh scene writer depends on a higher responsibility")
    (source / "Object3DFbxMeshSceneWrite.h").write_text('#include "FbxBinary.h"\n#include "Object3DFbxPoseWrite.h"\n', encoding="utf-8")
    (source / "Object3DFbxGeometryRegistry.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX geometry registry depends on a higher responsibility")
    (source / "Object3DFbxGeometryRegistry.h").write_text('#include "Object3DMesh.h"\n#include "FbxBinary.h"\n', encoding="utf-8")
    (source / "Object3DGlbMaterialVariant.h").write_text('#include "Object3DMaterial.h"\n', encoding="utf-8")
    check("GLB material data depends on another responsibility")
    (source / "Object3DGlbMaterialVariant.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DGlbMaterialPlan.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB material plan depends on a higher responsibility")
    (source / "Object3DGlbMaterialPlan.h").write_text('#include "Object3DGlbMaterialVariant.h"\n#include "Object3DMaterial.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DGlbMaterialWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB material writer depends on a higher responsibility")
    (source / "Object3DGlbMaterialWrite.h").write_text('#include "Object3DGlbMaterialVariant.h"\n#include "GlbBuffer.h"\n#include "Object3DData.h"\n', encoding="utf-8")
    (source / "Object3DGlbSceneWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB scene writer depends on a higher responsibility")
    (source / "Object3DGlbSceneWrite.h").write_text('#include "GlbBuffer.h"\n#include "Object3DData.h"\n', encoding="utf-8")
    (source / "MeshTangents.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Mesh tangent computation depends on a higher responsibility")
    (source / "MeshTangents.h").write_text('#include "MeshTopologyCore.h"\n', encoding="utf-8")
    (source / "Object3DGlbTangentData.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB tangent data depends on a higher responsibility")
    (source / "Object3DGlbTangentData.h").write_text('#include "MeshTangents.h"\n', encoding="utf-8")
    (source / "Object3DGlbTangentPlan.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB tangent plan depends on a higher responsibility")
    (source / "Object3DGlbTangentPlan.h").write_text('#include "Object3DGlbTangentData.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DGlbPointAttributeWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB point attribute writer depends on a higher responsibility")
    (source / "Object3DGlbPointAttributeWrite.h").write_text('#include "GlbBuffer.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DGlbTangentWrite.h").write_text('#include "Object3DGlbTangentPlan.h"\n', encoding="utf-8")
    check("GLB tangent writer depends on a higher responsibility")
    (source / "Object3DGlbTangentWrite.h").write_text('#include "Object3DGlbTangentData.h"\n#include "Object3DGlbPointAttributeWrite.h"\n', encoding="utf-8")
    (source / "Object3DGlbSharedMeshWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB shared mesh writer depends on a higher responsibility")
    (source / "Object3DGlbSharedMeshWrite.h").write_text('#include "GlbBuffer.h"\n#include "Object3DGlbPointAttributeWrite.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DGlbMeshExtrasWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB mesh extras writer depends on a higher responsibility")
    (source / "Object3DGlbMeshExtrasWrite.h").write_text('#include "GlbBuffer.h"\n#include "Object3DGlbPointIdentity.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DGlbPrimitivePlan.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB primitive planner depends on a higher responsibility")
    (source / "Object3DGlbPrimitivePlan.h").write_text('#include "Object3DMaterial.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DFbxTangentProjection.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX tangent projection depends on a higher responsibility")
    (source / "Object3DFbxTangentProjection.h").write_text('#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DGlbFbxTangentRoute.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB FBX tangent route depends on a higher responsibility")
    (source / "Object3DGlbFbxTangentRoute.h").write_text('#include "Object3DFbxTangentProjection.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DGlbMeshWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB mesh writer depends on a higher responsibility")
    (source / "Object3DGlbMeshWrite.h").write_text('#include "Object3DGlbPrimitivePlan.h"\n#include "Object3DGlbFbxTangentRoute.h"\n#include "Object3DGlbMaterialPlan.h"\n', encoding="utf-8")
    (source / "Object3DGlbSkinWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB skin writer depends on a higher responsibility")
    (source / "Object3DGlbSkinWrite.h").write_text('#include "GlbBuffer.h"\n#include "Transform.h"\n', encoding="utf-8")
    (source / "Object3DGlbContainerWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB container writer depends on a higher responsibility")
    (source / "Object3DGlbContainerWrite.h").write_text('#include "GlbBuffer.h"\n', encoding="utf-8")
    (source / "Object3DGlbDocumentWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB document writer depends on a higher responsibility")
    (source / "Object3DGlbDocumentWrite.h").write_text('#include "GlbBuffer.h"\n#include "Object3DGlbSceneWrite.h"\n#include "Object3DGlbMaterialWrite.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceMeshes.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface mesh assembly depends on a higher responsibility")
    (source / "Object3DTemplateSurfaceMeshes.h").write_text('#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceMaterialRouting.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface material routing depends on a higher responsibility")
    (source / "Object3DTemplateSurfaceMaterialRouting.h").write_text('#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceMaterialNative.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface material adapter depends on a higher responsibility")
    (source / "Object3DTemplateSurfaceMaterialNative.h").write_text('#include "Object3DTemplateSurfaceMaterialRouting.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceResources.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface resources depend on a higher responsibility")
    (source / "Object3DTemplateSurfaceResources.h").write_text('#include "Object3DData.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceReplacementResources.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface replacement resource planning depends on a higher responsibility")
    (source / "Object3DTemplateSurfaceReplacementResources.h").write_text('#include "Object3DData.h"\n#include "Object3DTemplateSurfaceResources.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceReplacementLayers.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface replacement layer processing depends on a higher responsibility")
    (source / "Object3DTemplateSurfaceReplacementLayers.h").write_text('#include "Object3DAdjustmentData.h"\n#include "Object3DTemplateSurfaceReplacementResources.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceSlots.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface slot planning depends on a higher responsibility")
    (source / "Object3DTemplateSurfaceSlots.h").write_text('#include "Motion.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceEditRemap.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface edit remapping depends on a higher responsibility")
    (source / "Object3DTemplateSurfaceEditRemap.h").write_text('#include "MeshPointMask.h"\n', encoding="utf-8")
    (source / "Object3DSelectionData.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Point selection data depends on a higher responsibility")
    (source / "Object3DSelectionData.h").write_text('#include "MeshPointMask.h"\n', encoding="utf-8")
    (source / "MeshPointMaskValidation.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Point mask validation depends on a higher responsibility")
    (source / "MeshPointMaskValidation.h").write_text('#include "MeshPointMask.h"\n', encoding="utf-8")
    (source / "Object3DSelectionState.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Point selection validation depends on a higher responsibility")
    (source / "Object3DSelectionState.h").write_text('#include "Object3DSelectionValidation.h"\n', encoding="utf-8")
    (source / "Object3DSelectionValidation.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Point selection validation depends on a higher responsibility")
    (source / "Object3DSelectionValidation.h").write_text('#include "Object3DSelectionData.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceSelectionRemap.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface selection remapping depends on a higher responsibility")
    (source / "Object3DTemplateSurfaceSelectionRemap.h").write_text('#include "Object3DSelectionData.h"\n#include "Object3DTemplateSurfaceEditRemap.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceReferences.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface references depend on a higher responsibility")
    (source / "Object3DTemplateSurfaceReferences.h").write_text('#include "Object3DTemplateTypes.h"\n#include "SurfaceBindingCore.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceFollowers.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface followers depend on a higher responsibility")
    (source / "Object3DTemplateSurfaceFollowers.h").write_text('#include "Object3DData.h"\n#include "SurfaceBindingCore.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceFollowerNative.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface follower adapter depends on a higher responsibility")
    (source / "Object3DTemplateSurfaceFollowerNative.h").write_text('#include "Object3DTemplateSurfaceFollowers.h"\n', encoding="utf-8")
    (source / "Object3DGlbPointIdentity.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB point identity codec depends on a higher responsibility")
    (source / "Object3DGlbPointIdentity.h").write_text('#include "MeshData.h"\n', encoding="utf-8")
    (source / "Object3DGlbPointRestoreCore.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB point restoration depends on a higher responsibility")
    (source / "Object3DGlbPointRestoreCore.h").write_text('#include "Object3DGlbPointIdentity.h"\n#include "MeshTopologyCore.h"\n', encoding="utf-8")
    (source / "GlbDocument.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB document or image interface depends on a higher responsibility")
    (source / "GlbDocument.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "GlbImageResolver.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB document or image interface depends on a higher responsibility")
    (source / "GlbImageResolver.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DGlbImportOptions.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB import options depend on a higher responsibility")
    (source / "Object3DGlbImportOptions.h").write_text('#include "GlbDocument.h"\n', encoding="utf-8")
    (source / "Object3DGlbJson.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB JSON fields depend on a higher responsibility")
    (source / "Object3DGlbJson.h").write_text('#include "Object3DGlbImportOptions.h"\n', encoding="utf-8")
    (source / "Object3DGlbAccessorRead.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB accessor reader depends on a higher responsibility")
    (source / "Object3DGlbAccessorRead.h").write_text('#include "GlbDocument.h"\n#include "Object3DGlbJson.h"\n', encoding="utf-8")
    (source / "Object3DGlbGeometryBudget.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB geometry budget depends on a higher responsibility")
    (source / "Object3DGlbGeometryBudget.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DGlbTransform.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB transform depends on a higher responsibility")
    (source / "Object3DGlbTransform.h").write_text('#include "Object3DGlbJson.h"\n#include "Transform.h"\n', encoding="utf-8")
    (source / "Object3DGlbMeshBundle.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB mesh bundle depends on a higher responsibility")
    (source / "Object3DGlbMeshBundle.h").write_text('#include "Object3DGlbGeometryBudget.h"\n#include "Object3DGlbMeshAttributes.h"\n', encoding="utf-8")
    (source / "Object3DGlbPrimitiveTopology.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB primitive topology depends on a higher responsibility")
    (source / "Object3DGlbPrimitiveTopology.h").write_text('#include "MeshTopologyCore.h"\n', encoding="utf-8")
    (source / "Object3DGlbTangentNotes.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB tangent notes depend on a higher responsibility")
    (source / "Object3DGlbTangentNotes.h").write_text('#include "Object3DGlbJson.h"\n', encoding="utf-8")
    (source / "Object3DGlbBundleData.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB bundle data depends on a higher responsibility")
    (source / "Object3DGlbBundleData.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DGlbBundleIndex.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB bundle index depends on a higher responsibility")
    (source / "Object3DGlbBundleIndex.h").write_text('#include "Object3DGlbBundleData.h"\n', encoding="utf-8")
    (source / "Object3DGlbTangentSnapshot.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB tangent snapshot depends on a higher responsibility")
    (source / "Object3DGlbTangentSnapshot.h").write_text('#include "MeshTopologyCore.h"\n', encoding="utf-8")
    (source / "Object3DGlbMeshAttributes.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB mesh attribute parser depends on a higher responsibility")
    (source / "Object3DGlbMeshAttributes.h").write_text('#include "Object3DGlbJson.h"\n#include "MeshData.h"\n', encoding="utf-8")
    (source / "Object3DProjectCodec.h").write_text('#include "Object3DProject.h"\n', encoding="utf-8")
    check("Project codec depends on a higher responsibility")
    (source / "Object3DProjectCodec.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DProjectVersion.h").write_text('#include "Object3DProject.h"\n', encoding="utf-8")
    check("Project version selection depends on a higher responsibility")
    (source / "Object3DProjectVersion.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DProjectAdjustmentValidation.h").write_text('#include "Object3DEditor.h"\n', encoding="utf-8")
    check("Project adjustment validation depends on a higher responsibility")
    (source / "Object3DProjectAdjustmentValidation.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DProjectRigArchive.h").write_text('#include "Object3DEditor.h"\n', encoding="utf-8")
    check("Project rig archive depends on a higher responsibility")
    (source / "Object3DProjectRigArchive.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DProjectRigRestore.h").write_text('#include "Object3DEditor.h"\n', encoding="utf-8")
    check("Focused Object3DProjectRigRestore.h includes higher responsibility")
    (source / "Object3DProjectRigRestore.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DExternalIdentity.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("External identity data depends on a higher responsibility")
    (source / "Object3DExternalIdentity.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DExternalProtocol.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("External identity protocol depends on another responsibility")
    (source / "Object3DExternalProtocol.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DFbxExternalIdentityRead.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX external identity reader depends on a higher responsibility")
    (source / "Object3DFbxExternalIdentityRead.h").write_text('#include "Object3DExternalIdentity.h"\n', encoding="utf-8")
    (source / "Object3DFbxSceneMetadata.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX scene metadata reader depends on a higher responsibility")
    (source / "Object3DFbxSceneMetadata.h").write_text('#include "RigSampleCatalogCore.h"\n', encoding="utf-8")
    (source / "Object3DFbxExternalIdentityWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX external identity writer depends on a higher responsibility")
    (source / "Object3DFbxExternalIdentityWrite.h").write_text('#include "FbxBinary.h"\n', encoding="utf-8")
    (source / "Object3DFbxSceneMetadataWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX scene metadata writer depends on a higher responsibility")
    (source / "Object3DFbxSceneMetadataWrite.h").write_text('#include "FbxBinary.h"\n', encoding="utf-8")
    (source / "Object3DFbxProgressiveData.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX progressive plan data depends on a higher responsibility")
    (source / "Object3DFbxProgressiveData.h").write_text('#include "ProgressiveMorphData.h"\n', encoding="utf-8")
    (source / "Object3DMorphChannels.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Logical morph channels depend on a higher responsibility")
    (source / "Object3DMorphChannels.h").write_text('#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "Object3DFbxMorphChannelsWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX morph channel writer depends on a higher responsibility")
    (source / "Object3DFbxMorphChannelsWrite.h").write_text('#include "Object3DFbxProgressiveData.h"\n#include "Object3DMesh.h"\n#include "FbxMorphWrite.h"\n', encoding="utf-8")
    (source / "Object3DFbxSharedGeometry.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX shared geometry comparison depends on a higher responsibility")
    (source / "Object3DFbxSharedGeometry.h").write_text('#include "Object3DMesh.h"\n#include "FbxBinary.h"\n', encoding="utf-8")
    (source / "Object3DProjectArchive.h").write_text('#include "Object3DEditor.h"\n', encoding="utf-8")
    check("Project archive depends on a higher responsibility")
    (source / "Object3DProjectArchive.h").write_text('#include "Object3DAdjustmentData.h"\n#include "SurfaceBindingCore.h"\n', encoding="utf-8")
    (source / "SurfaceBindingCore.h").write_text('#include "Object3DEditor.h"\n', encoding="utf-8")
    check("Focused Object3DTemplateSurfaceReferences.h includes higher responsibility")
    # Keep the reference planner out of this particular path so the archive's
    # transitive guard is the one exercised by the injected editor dependency.
    (source / "Object3DTemplateSurfaceReferences.h").write_text('#include "Object3DTemplateTypes.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceFollowers.h").write_text('#include "Object3DData.h"\n', encoding="utf-8")
    check("Focused Object3DProjectArchive.h includes higher responsibility")
    (source / "SurfaceBindingCore.h").write_text('#include "Transform.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceReferences.h").write_text('#include "Object3DTemplateTypes.h"\n#include "SurfaceBindingCore.h"\n', encoding="utf-8")
    (source / "Object3DTemplateSurfaceFollowers.h").write_text('#include "Object3DData.h"\n#include "SurfaceBindingCore.h"\n', encoding="utf-8")
    (source / "Runtime.h").write_text('#include "Object3DEditor.h"\n', encoding="utf-8")
    check("Core responsibility boundary")
    (source / "Runtime.h").write_text('#include "Object3DProgressiveMorphCore.h"\n', encoding="utf-8")
    check("Core responsibility boundary")
    (source / "Runtime.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "FbxBinary.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX binary writer depends on another responsibility")
    (source / "FbxBinary.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "GlbBuffer.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB buffer writer depends on a higher responsibility")
    (source / "GlbBuffer.h").write_text('#include "MeshData.h"\n', encoding="utf-8")
    (source / "Object3DGlbAnimationOptions.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB animation options depend on a higher responsibility")
    (source / "Object3DGlbAnimationOptions.h").write_text('#include "Motion.h"\n', encoding="utf-8")
    (source / "Object3DGlbAnimationWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB animation writer depends on a higher responsibility")
    (source / "Object3DGlbAnimationWrite.h").write_text('#include "Object3DGlbAnimationOptions.h"\n#include "GlbBuffer.h"\n#include "Object3DData.h"\n', encoding="utf-8")
    (source / "FbxSceneNodes.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX scene nodes depend on a higher responsibility")
    (source / "FbxSceneNodes.h").write_text('#include "FbxBinary.h"\n#include "Transform.h"\n', encoding="utf-8")
    (source / "FbxAnimationNodes.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX animation nodes depend on a higher responsibility")
    (source / "FbxAnimationNodes.h").write_text('#include "FbxSceneNodes.h"\n#include "FbxBinary.h"\n#include "Transform.h"\n', encoding="utf-8")
    (source / "FbxTangentData.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX tangent data depends on a higher responsibility")
    (source / "FbxTangentData.h").write_text('#include "Transform.h"\n', encoding="utf-8")
    (source / "FbxTangentRead.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX tangent reader depends on a higher responsibility")
    (source / "FbxTangentRead.h").write_text('#include "FbxMetadata.h"\n#include "MeshTopologyCore.h"\n', encoding="utf-8")
    (source / "ProgressiveMorphData.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Morph or animation data depends on another responsibility")
    (source / "ProgressiveMorphData.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "FbxAnimationData.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Morph or animation data depends on another responsibility")
    (source / "FbxAnimationData.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "FbxProgressiveMetadata.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX progressive metadata depends on a higher responsibility")
    (source / "FbxProgressiveMetadata.h").write_text('#include "FbxMetadata.h"\n', encoding="utf-8")
    (source / "FbxMorphChannelRead.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX morph channels depend on a higher responsibility")
    (source / "FbxMorphChannelRead.h").write_text('#include "FbxAnimationData.h"\n#include "ProgressiveMorphData.h"\n#include "FbxProgressiveMetadata.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "FbxGeometryWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX geometry writer depends on a higher responsibility")
    (source / "FbxGeometryWrite.h").write_text('#include "FbxBinary.h"\n#include "FbxSceneNodes.h"\n#include "FbxTangentData.h"\n#include "Object3DMesh.h"\n', encoding="utf-8")
    (source / "FbxGeometryRead.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX geometry reader depends on a higher responsibility")
    (source / "FbxGeometryRead.h").write_text('#include "Object3DMesh.h"\n#include "FbxSkin.h"\n#include "FbxTopology.h"\n', encoding="utf-8")
    (source / "Object3DMesh.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Mesh data depends on a higher responsibility")
    (source / "Object3DMesh.h").write_text('#include "MeshTopologyCore.h"\n', encoding="utf-8")
    (source / "GlbFlatNormals.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("GLB flat normals depend on a higher responsibility")
    (source / "GlbFlatNormals.h").write_text('#include "MeshTopologyCore.h"\n#include "Transform.h"\n', encoding="utf-8")
    (source / "TemplateSurfaceTopology.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Template surface topology depends on a higher responsibility")
    (source / "TemplateSurfaceTopology.h").write_text('#include "Object3DMesh.h"\n#include "SurfaceBindingCore.h"\n', encoding="utf-8")
    (source / "Object3DMaterial.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Material data depends on another responsibility")
    (source / "Object3DMaterial.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "FbxResourceWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX resource writer depends on a higher responsibility")
    (source / "FbxResourceWrite.h").write_text('#include "FbxSceneNodes.h"\n#include "FbxBinary.h"\n#include "Object3DMaterial.h"\n', encoding="utf-8")
    (source / "FbxMetadata.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX metadata decoder depends on another responsibility")
    (source / "FbxMetadata.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "FbxTopology.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX topology metadata depends on a higher responsibility")
    (source / "FbxTopology.h").write_text('#include "FbxBinary.h"\n#include "FbxMetadata.h"\n#include "MeshData.h"\n', encoding="utf-8")
    (source / "FbxImageResolver.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX image interface depends on another responsibility")
    (source / "FbxImageResolver.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "FbxResources.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX resource reader depends on a higher responsibility")
    (source / "FbxResources.h").write_text('#include "FbxImageResolver.h"\n#include "FbxMetadata.h"\n#include "Object3DData.h"\n', encoding="utf-8")
    (source / "FbxSkeleton.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX skeleton reader depends on a higher responsibility")
    (source / "FbxSkeleton.h").write_text('#include "FbxMetadata.h"\n#include "Object3DData.h"\n', encoding="utf-8")
    (source / "FbxSkin.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX skin reader depends on a higher responsibility")
    (source / "FbxSkin.h").write_text('#include "FbxMetadata.h"\n#include "MeshData.h"\n', encoding="utf-8")
    (source / "Object3DFbxSkinData.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX skin data depends on a higher responsibility")
    (source / "Object3DFbxSkinData.h").write_text('#include "Transform.h"\n', encoding="utf-8")
    (source / "Object3DFbxSkeletonWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX skeleton writer depends on a higher responsibility")
    (source / "Object3DFbxSkeletonWrite.h").write_text('#include "Object3DFbxSkinData.h"\n#include "Object3DFbxPoseWrite.h"\n#include "FbxBinary.h"\n', encoding="utf-8")
    (source / "FbxSkinWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX skin writer depends on a higher responsibility")
    (source / "FbxSkinWrite.h").write_text('#include "FbxBinary.h"\n#include "FbxSceneNodes.h"\n#include "Object3DFbxSkinData.h"\n#include "MeshData.h"\n', encoding="utf-8")
    (source / "FbxMorphShape.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX morph shape reader depends on a higher responsibility")
    (source / "FbxMorphShape.h").write_text('#include "FbxMetadata.h"\n#include "MeshTopologyCore.h"\n', encoding="utf-8")
    (source / "FbxMorphWrite.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("FBX morph writer depends on a higher responsibility")
    (source / "FbxMorphWrite.h").write_text('#include "FbxBinary.h"\n#include "FbxSceneNodes.h"\n#include "MeshTopologyCore.h"\n', encoding="utf-8")
    (source / "Motion.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Skeletal motion depends on a higher responsibility")
    (source / "Motion.h").write_text('#include "Transform.h"\n', encoding="utf-8")
    (source / "MeshGeometryCore.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Mesh foundation depends on a higher responsibility")
    (source / "MeshGeometryCore.h").write_text('#include "MeshData.h"\n', encoding="utf-8")
    (source / "Object3DData.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Asset data depends on a higher responsibility")
    (source / "Object3DData.h").write_text('#include "MeshTopologyCore.h"\n', encoding="utf-8")
    (source / "SkeletalMeshCore.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Deformation depends on a higher responsibility")
    (source / "SkeletalMeshCore.h").write_text('#include "MeshData.h"\n#include "Motion.h"\n', encoding="utf-8")
    # Even a group cycle through otherwise permitted edges must be rejected.
    (source / "Runtime.h").write_text('#include "Exchange.h"\n', encoding="utf-8")
    exchange = source / "Exchange.cpp"
    exchange.write_text('#include "Runtime.h"\n', encoding="utf-8")
    cycle_result = subprocess.run([sys.executable, "-B", str(checker), str(root)], capture_output=True, text=True)
    if not cycle_result.returncode or "Core responsibility group cycle:" not in (cycle_result.stdout + cycle_result.stderr):
        raise AssertionError("Core group cycle was not rejected: " + cycle_result.stdout + cycle_result.stderr)
    (source / "Runtime.h").write_text("#pragma once\n", encoding="utf-8")
    exchange.write_text('#include "Exchange.h"\n', encoding="utf-8")
    rig = source / "Rig.cpp"
    rig.write_text('#include "Exchange.h"\n', encoding="utf-8")
    check("Core responsibility boundary")
    rig.write_text('#include "Object3DProgressiveMorph.h"\n', encoding="utf-8")
    check("compatibility aggregate")
    rig.write_text('#include "Object3DProgressiveMorphCore.h"\n', encoding="utf-8")
    exchange.write_text('#include "Object3DProject.h"\n', encoding="utf-8")
    check("Core responsibility boundary")
    exchange.write_text('#include "Exchange.h"\n', encoding="utf-8")
    utility = source / "Transform.h"
    utility.write_text('#include "Runtime.h"\n', encoding="utf-8")
    check("Core utility depends on a higher responsibility")
    utility.write_text("#pragma once\n", encoding="utf-8")
    (source / "SurfaceBindingCore.h").write_text('#include "SkeletalMesh.h"\n', encoding="utf-8")
    check("Core responsibility boundary")
    (source / "SurfaceBindingCore.h").write_text('#include "Object3DProgressiveMorphCore.h"\n', encoding="utf-8")
    (source / "Runtime.h").write_text('#include "SkeletalMesh.h"\n', encoding="utf-8")
    check("Focused SurfaceBindingCore.h includes higher responsibility")
    (source / "SurfaceBindingCore.h").write_text('#include "Transform.h"\n', encoding="utf-8")
    (source / "Object3DProgressiveMorphCore.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DInstanceTransferCore.h").write_text('#include "SkeletalMesh.h"\n', encoding="utf-8")
    (source / "SkeletalMesh.h").write_text('#include "Object3DEditor.h"\n', encoding="utf-8")
    check("Focused Object3DInstanceTransferCore.h includes higher responsibility")
    (source / "Object3DInstanceTransferCore.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "SkeletalMesh.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Object3DProgressiveMorphCore.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    # A direct dependency on runtime is allowed, but a transitive dependency on
    # project storage through that runtime header must still be rejected.
    (source / "Runtime.h").write_text('#include "SkeletalMesh.h"\n', encoding="utf-8")
    (source / "SkeletalMesh.h").write_text('#include "Object3DProject.h"\n', encoding="utf-8")
    check("Focused Object3DProgressiveMorphCore.h includes higher responsibility")
    (source / "SkeletalMesh.h").write_text("#pragma once\n", encoding="utf-8")
    (source / "Runtime.h").write_text('#include "SkeletalMesh.h"\n', encoding="utf-8")
    # The geometry API must also reject an indirect dependency on runtime mesh classes.
    (source / "MeshTopologyCore.h").write_text('#include "Runtime.h"\n', encoding="utf-8")
    # Remove the progressive path so this check isolates the topology boundary.
    (source / "Object3DProgressiveMorphCore.h").write_text("#pragma once\n", encoding="utf-8")
    check("Mesh foundation depends on a higher responsibility")

print("Responsibility boundaries reject direct, transitive, and aggregate regressions")
