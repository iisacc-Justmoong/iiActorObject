cmake_minimum_required(VERSION 3.24)
project(iiActorObject VERSION 0.2.1 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
        set(_default_home "$ENV{HOME}")
    elseif(DEFINED ENV{USERPROFILE} AND NOT "$ENV{USERPROFILE}" STREQUAL "")
        set(_default_home "$ENV{USERPROFILE}")
    else()
        message(FATAL_ERROR "Set CMAKE_INSTALL_PREFIX when HOME and USERPROFILE are unavailable")
    endif()
    set(CMAKE_INSTALL_PREFIX "${_default_home}/.local/SDK/iiActorObject" CACHE PATH "Install prefix" FORCE)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(CTest)
option(BUILD_SHARED_LIBS "Build iiActorObject as a shared library" ON)
option(IIACTOROBJECT_WITH_NSCHARACTER "Build character-specific views and humanoid recipes above the core 3D engine" ON)
option(IIACTOROBJECT_WITH_DNA "Build the MIT OpenRigLogic DNA reader, writer and evaluator" ON)
option(IIACTOROBJECT_WITH_VISUALCODEC "Build the optional iiVisualCodec external-image adapter" OFF)
set(IIACTOROBJECT_HAS_NSCHARACTER ${IIACTOROBJECT_WITH_NSCHARACTER})
set(IIACTOROBJECT_HAS_DNA ${IIACTOROBJECT_WITH_DNA})

# Keep source files grouped by responsibility while preserving the existing
# <iiActorObject/...> include spellings for build-tree and installed consumers.
set(_iiActorObject_source_groups Core Runtime Geometry Rig Project Authoring
    Interchange Interchange/FBX Interchange/GLB)
set(_iiActorObject_source_dirs)
foreach(_group IN LISTS _iiActorObject_source_groups)
    set(_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/${_group}")
    list(APPEND _iiActorObject_source_dirs "${_dir}")
    file(GLOB _headers CONFIGURE_DEPENDS "${_dir}/*.h")
    foreach(_header IN LISTS _headers)
        get_filename_component(_name "${_header}" NAME)
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/${_name}"
            CONTENT "#pragma once\n#include \"${_header}\"\n")
    endforeach()
endforeach()
file(GLOB _character_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/NSCharacter/*.h")
foreach(_header IN LISTS _character_headers)
    get_filename_component(_name "${_header}" NAME)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/NSCharacter/${_name}"
        CONTENT "#pragma once\n#include \"${_header}\"\n")
endforeach()

add_library(iiActorObject src/Runtime/Actor.cpp
    src/Interchange/GLB/GlbAnimationSource.cpp
    src/Interchange/GLB/GlbAnimationPose.cpp
    src/Interchange/GLB/GlbAnimationBake.cpp
    src/Rig/Object3DAnimation.cpp
    src/Project/Object3DProjectAnimation.cpp
    src/Interchange/Object3DProxy.cpp
    src/Rig/Object3DBvh.cpp
    src/Rig/Object3DPoseRig.cpp
    src/Rig/Object3DPoseCorrectives.cpp
    src/Rig/Object3DAutomaticCorrectives.cpp
    src/Rig/Object3DPoseCorrectiveAnimation.cpp
    src/Rig/Object3DPoseMorphs.cpp
    src/Rig/Object3DPoseSnapshot.cpp
    src/Rig/Object3DRigSnapshot.cpp
    src/Rig/Object3DRigSamples.cpp
    src/Runtime/ActorComponent.cpp src/Runtime/SceneComponent.cpp src/Interchange/Usd.cpp
    src/Runtime/SkeletalMeshComponent.cpp
    src/Runtime/Character.cpp
    src/Runtime/Object3DAsset.cpp
    src/Authoring/Object3DTemplate.cpp
    src/Authoring/Object3DTemplateMorphs.cpp
    src/Rig/Object3DProgressiveMorph.cpp
    src/Rig/Object3DProgressiveMorphAnimation.cpp
    src/Authoring/Object3DTemplateRigTransfer.cpp
    src/Authoring/Object3DTemplateRigProjection.cpp
    src/Geometry/Object3DPointCorrespondence.cpp
    src/Authoring/Object3DTemplateShapeFit.cpp
    src/Authoring/Object3DTemplateAdapt.cpp
    src/Authoring/Object3DTemplateAdaptFollowers.cpp
    src/Interchange/Object3DObj.cpp
    src/Runtime/Object3DEngine.cpp
    src/Authoring/Object3DEngineAuthoring.cpp
    src/Rig/Object3DEngineRig.cpp
    src/Authoring/Object3DEditor.cpp
    src/Authoring/Object3DSelections.cpp
    src/Authoring/Object3DAdjustment.cpp
    src/Authoring/Object3DEditLayers.cpp
    src/Authoring/Object3DTemplateSurfaceSwap.cpp
    src/Project/Object3DProject.cpp
    src/Rig/Object3DRig.cpp
    src/Authoring/Object3DEditorRig.cpp
    src/Runtime/Object3DInstanceTransfer.cpp
    src/Authoring/Object3DEditorInstanceTransfer.cpp
    src/Runtime/Object3DJointIdentityInternal.cpp
    src/Interchange/Object3DDna.cpp
    src/Geometry/MeshPrimitives.cpp
    src/Geometry/ImplicitSurface.cpp
    src/Rig/SkinBinding.cpp
    src/Rig/SkinEnvelopeInference.cpp
    src/Rig/SkinSurfaceBinding.cpp
    src/Geometry/MeshSurface.cpp
    src/Geometry/MeshSurfaceCorrespondence.cpp
    src/Geometry/MeshCollider.cpp
    src/Geometry/SurfaceFitting.cpp
    src/Geometry/SurfaceFittingRules.cpp
    src/Geometry/SurfaceBinding.cpp
    src/Rig/SurfaceAttachment.cpp
    src/Geometry/MeshEditing.cpp
    src/Geometry/MeshScreenSelection.cpp
    src/Interchange/MeshScreenSelectionAsset.cpp
    src/Geometry/MeshMaskProjection.cpp
    src/Interchange/MeshMaskProjectionAsset.cpp
    src/Geometry/MeshSymmetry.cpp
    src/Geometry/MorphSymmetry.cpp
    src/Rig/SkinPainting.cpp
    src/Rig/SkeletonEditing.cpp
    src/Rig/SkeletonScale.cpp
    src/Interchange/Object3DExternalEdits.cpp
    src/Interchange/GLB/Object3DGlbImport.cpp
    src/Interchange/GLB/Object3DGlbMaterials.cpp
    src/Interchange/GLB/Object3DGlbScene.cpp
    src/Interchange/GLB/Object3DGlbSkin.cpp
    src/Interchange/GLB/Object3DGlbMesh.cpp
    src/Interchange/GLB/Object3DGlbPointRestore.cpp
    src/Interchange/GLB/Object3DGlbExternalEdits.cpp
    src/Interchange/Object3DInterchange.cpp
    src/Interchange/RigSampleCatalogExport.cpp
    src/Interchange/FBX/Object3DFbxTangents.cpp
    src/Interchange/FBX/Object3DFbxAnimation.cpp
    src/Interchange/FBX/Object3DFbxAnimationImport.cpp
    src/Interchange/FBX/Object3DFbxProgressivePlan.cpp
    src/Interchange/FBX/Object3DFbx.cpp
    src/Interchange/FBX/Object3DFbxImport.cpp
    src/third_party/ufbx/ufbx.cpp
    src/third_party/mikktspace/mikktspace.cpp)

# Compile embedded components against the same export and visibility contract
# as the final library, without coupling the lower-level component to it.
function(iiActorObjectConfigureEmbeddedTarget target)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/include" ${_iiActorObject_source_dirs})
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PRIVATE iiActorObject_EXPORTS)
    else()
        target_compile_definitions(${target} PRIVATE IIACTOROBJECT_STATIC_DEFINE)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /Zc:__cplusplus /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
endfunction()

add_library(iiActorObjectFoundation OBJECT
    src/Core/Transform.cpp src/Core/MeshData.cpp src/Core/Object3DNames.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFoundation>)

# Owned surface fitting parameters do not depend on editor or engine sessions.
add_library(iiActorObjectSurfaceFitSettings OBJECT src/Geometry/SurfaceFitSettings.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectSurfaceFitSettings)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectSurfaceFitSettings>)

# GLB byte buffers and accessor declarations do not own assets or scenes.
add_library(iiActorObjectGlbBuffer OBJECT src/Interchange/GLB/Object3DGlbBuffer.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbBuffer)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbBuffer>)
add_library(iiActorObjectGlbSkinWrite OBJECT src/Interchange/GLB/Object3DGlbSkinWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbSkinWrite)
add_dependencies(iiActorObjectGlbSkinWrite iiActorObjectGlbBuffer iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbSkinWrite>)
add_library(iiActorObjectGlbContainerWrite OBJECT src/Interchange/GLB/Object3DGlbContainerWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbContainerWrite)
add_dependencies(iiActorObjectGlbContainerWrite iiActorObjectGlbBuffer)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbContainerWrite>)

# FBX 7.4 byte encoding is independent of scene, asset and engine ownership.
add_library(iiActorObjectFbxBinary OBJECT src/Interchange/FBX/Object3DFbxBinary.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxBinary)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxBinary>)

# FBX model, property and layer nodes depend only on binary values and transforms.
add_library(iiActorObjectFbxSceneNodes OBJECT src/Interchange/FBX/Object3DFbxSceneNodes.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSceneNodes)
add_dependencies(iiActorObjectFbxSceneNodes iiActorObjectFbxBinary iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes>)

# Bind-pose and mesh-model nodes are shared by the scene exporters.
add_library(iiActorObjectFbxPoseWrite OBJECT src/Interchange/FBX/Object3DFbxPoseWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxPoseWrite)
add_dependencies(iiActorObjectFbxPoseWrite iiActorObjectFbxBinary iiActorObjectFbxSceneNodes)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxPoseWrite>)
add_library(iiActorObjectFbxMeshSceneWrite OBJECT src/Interchange/FBX/Object3DFbxMeshSceneWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMeshSceneWrite)
add_dependencies(iiActorObjectFbxMeshSceneWrite iiActorObjectFbxBinary iiActorObjectFbxSceneNodes iiActorObjectFbxPoseWrite)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxMeshSceneWrite>)

# A sampled FBX take can be written without asset, rig session or engine ownership.
add_library(iiActorObjectFbxAnimationNodes OBJECT src/Interchange/FBX/Object3DFbxAnimationNodes.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxAnimationNodes)
add_dependencies(iiActorObjectFbxAnimationNodes iiActorObjectFbxBinary iiActorObjectFbxSceneNodes iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxAnimationNodes>)

# Core skeletal and animation math needs the foundation only; runtime rig
# sessions and engine ownership remain in their own higher-level code.
add_library(iiActorObjectSkeletalMotion OBJECT
    src/Rig/Skeleton.cpp src/Rig/Animation.cpp
    src/Rig/CharacterRig.cpp src/Rig/PoseAiming.cpp
    src/Rig/PoseFrames.cpp src/Rig/PoseReference.cpp
    src/Rig/PoseRetarget.cpp src/Rig/AnimationRetarget.cpp
    src/Rig/RigSampleCatalog.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectSkeletalMotion)
add_dependencies(iiActorObjectSkeletalMotion iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectSkeletalMotion>)

# Mesh source IDs and asset DTOs do not own actors or editing sessions.
add_library(iiActorObjectMeshFoundation OBJECT
    src/Geometry/MeshGeometry.cpp src/Geometry/MeshTopology.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectMeshFoundation)
add_dependencies(iiActorObjectMeshFoundation iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)

# Validate and stage FBX clips using skeleton and mesh values, without engine ownership.
add_library(iiActorObjectFbxAnimationStage OBJECT src/Interchange/FBX/Object3DFbxAnimationStage.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxAnimationStage)
add_dependencies(iiActorObjectFbxAnimationStage iiActorObjectSkeletalMotion iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxAnimationStage>)

# Mesh partitions and FBX corner projections depend on topology data, not assets.
add_library(iiActorObjectGlbPrimitivePlan OBJECT src/Interchange/GLB/Object3DGlbPrimitivePlan.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbPrimitivePlan)
add_dependencies(iiActorObjectGlbPrimitivePlan iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbPrimitivePlan>)
add_library(iiActorObjectFbxTangentProjection OBJECT src/Interchange/FBX/Object3DFbxTangentProjection.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxTangentProjection)
add_dependencies(iiActorObjectFbxTangentProjection iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxTangentProjection>)
add_library(iiActorObjectGlbFbxTangentRoute OBJECT src/Interchange/GLB/Object3DGlbFbxTangentRoute.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbFbxTangentRoute)
add_dependencies(iiActorObjectGlbFbxTangentRoute iiActorObjectFbxTangentProjection iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbFbxTangentRoute>)

# Tangent generation and GLB tangent planning depend on mesh data, not asset ownership.
add_library(iiActorObjectMeshTangents OBJECT src/Geometry/MeshTangents.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectMeshTangents)
add_dependencies(iiActorObjectMeshTangents iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectMeshTangents>)
add_library(iiActorObjectGlbTangentPlan OBJECT src/Interchange/GLB/Object3DGlbTangentPlan.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTangentPlan)
add_dependencies(iiActorObjectGlbTangentPlan iiActorObjectMeshTangents iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbTangentPlan>)
add_library(iiActorObjectGlbPointAttributeWrite OBJECT src/Interchange/GLB/Object3DGlbPointAttributeWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbPointAttributeWrite)
add_dependencies(iiActorObjectGlbPointAttributeWrite iiActorObjectGlbBuffer iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbPointAttributeWrite>)
add_library(iiActorObjectGlbTangentWrite OBJECT src/Interchange/GLB/Object3DGlbTangentWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTangentWrite)
add_dependencies(iiActorObjectGlbTangentWrite iiActorObjectGlbPointAttributeWrite iiActorObjectGlbBuffer)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbTangentWrite>)

# Primitive triangles borrow staged point data and do not own materials or actors.
add_library(iiActorObjectGlbPrimitiveTopology OBJECT src/Interchange/GLB/Object3DGlbPrimitiveTopology.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbPrimitiveTopology)
add_dependencies(iiActorObjectGlbPrimitiveTopology iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbPrimitiveTopology>)
add_library(iiActorObjectGlbBundleIndex OBJECT src/Interchange/GLB/Object3DGlbBundleIndex.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbBundleIndex)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbBundleIndex>)
add_library(iiActorObjectGlbTangentSnapshot OBJECT src/Interchange/GLB/Object3DGlbTangentSnapshot.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTangentSnapshot)
add_dependencies(iiActorObjectGlbTangentSnapshot iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbTangentSnapshot>)

# GLB flat-normal generation consumes topology and explicit work budgets only.
add_library(iiActorObjectGlbFlatNormals OBJECT src/Interchange/GLB/Object3DGlbFlatNormals.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbFlatNormals)
add_dependencies(iiActorObjectGlbFlatNormals iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbFlatNormals>)

# Topology compatibility and follower remapping do not own template sessions.
add_library(iiActorObjectTemplateSurfaceTopology OBJECT src/Geometry/Object3DTemplateSurfaceTopology.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceTopology)
add_dependencies(iiActorObjectTemplateSurfaceTopology iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceTopology>)

# Mesh assembly consumes slot mappings and borrowed meshes, without engine ownership.
add_library(iiActorObjectTemplateSurfaceMeshes OBJECT src/Geometry/Object3DTemplateSurfaceMeshes.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceMeshes)
add_dependencies(iiActorObjectTemplateSurfaceMeshes iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceMeshes>)

# Slot selection and identity mapping need bone values, not editor sessions.
add_library(iiActorObjectTemplateSurfaceSlots OBJECT src/Geometry/Object3DTemplateSurfaceSlots.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceSlots)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceSlots>)

# Remap immutable edit and selection values without a live editor session.
add_library(iiActorObjectTemplateSurfaceEditRemap OBJECT src/Geometry/Object3DTemplateSurfaceEditRemap.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceEditRemap)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceEditRemap>)

# Immutable selection remapping uses only selection values and the edit remapper.
add_library(iiActorObjectTemplateSurfaceSelectionRemap OBJECT src/Geometry/Object3DTemplateSurfaceSelectionRemap.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceSelectionRemap)
add_dependencies(iiActorObjectTemplateSurfaceSelectionRemap iiActorObjectTemplateSurfaceEditRemap)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceSelectionRemap>)

# Template surface references use value data and topology, not editor sessions.
add_library(iiActorObjectTemplateSurfaceReferences OBJECT src/Geometry/Object3DTemplateSurfaceReferences.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceReferences)
add_dependencies(iiActorObjectTemplateSurfaceReferences iiActorObjectTemplateSurfaceTopology)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceReferences>)

# Follower scheduling and skin transfer orchestration borrow staged values and an operation interface.
add_library(iiActorObjectTemplateSurfaceFollowers OBJECT src/Geometry/Object3DTemplateSurfaceFollowers.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceFollowers)
add_dependencies(iiActorObjectTemplateSurfaceFollowers iiActorObjectTemplateSurfaceReferences)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceFollowers>)
add_library(iiActorObjectTemplateSurfaceFollowerNative OBJECT src/Geometry/Object3DTemplateSurfaceFollowerNative.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceFollowerNative)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceFollowerNative>)

add_library(iiActorObjectAssetData OBJECT src/Core/Object3DImage.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectAssetData)
add_dependencies(iiActorObjectAssetData iiActorObjectMeshFoundation iiActorObjectSkeletalMotion)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectAssetData>)

# Mask and named-selection validation operate on borrowed core values.
add_library(iiActorObjectMeshPointMaskValidation OBJECT
    src/Geometry/MeshPointMaskValidation.cpp src/Geometry/MeshPointWeightValidation.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectMeshPointMaskValidation)
add_dependencies(iiActorObjectMeshPointMaskValidation iiActorObjectAssetData)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectMeshPointMaskValidation>)
add_library(iiActorObjectSelectionValidation OBJECT src/Geometry/Object3DSelectionValidation.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectSelectionValidation)
add_dependencies(iiActorObjectSelectionValidation iiActorObjectMeshPointMaskValidation iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectSelectionValidation>)
add_library(iiActorObjectProjectHistorySelection OBJECT src/Project/Object3DProjectHistorySelection.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectHistorySelection)
add_dependencies(iiActorObjectProjectHistorySelection iiActorObjectSelectionValidation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectProjectHistorySelection>)

# GLB material variants and resources borrow mesh, material and image data.
add_library(iiActorObjectGlbMaterialPlan OBJECT src/Interchange/GLB/Object3DGlbMaterialPlan.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMaterialPlan)
add_dependencies(iiActorObjectGlbMaterialPlan iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbMaterialPlan>)
add_library(iiActorObjectGlbMaterialWrite OBJECT src/Interchange/GLB/Object3DGlbMaterialWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMaterialWrite)
add_dependencies(iiActorObjectGlbMaterialWrite iiActorObjectGlbBuffer iiActorObjectAssetData)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbMaterialWrite>)

# GLB scene and skin JSON is serialized from caller-owned data.
add_library(iiActorObjectGlbSceneWrite OBJECT src/Interchange/GLB/Object3DGlbSceneWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbSceneWrite)
add_dependencies(iiActorObjectGlbSceneWrite iiActorObjectGlbBuffer iiActorObjectAssetData)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbSceneWrite>)
add_library(iiActorObjectGlbDocumentWrite OBJECT src/Interchange/GLB/Object3DGlbDocumentWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbDocumentWrite)
add_dependencies(iiActorObjectGlbDocumentWrite iiActorObjectGlbSceneWrite iiActorObjectGlbMaterialWrite iiActorObjectGlbBuffer)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbDocumentWrite>)

# Source-point identity sidecars use mesh and bone data, without owning an asset.
add_library(iiActorObjectGlbPointIdentity OBJECT src/Interchange/GLB/Object3DGlbPointIdentity.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbPointIdentity)
add_dependencies(iiActorObjectGlbPointIdentity iiActorObjectMeshFoundation iiActorObjectSkeletalMotion)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbPointIdentity>)

# Shared mesh attributes and mesh metadata use borrowed mesh data and buffer writers.
add_library(iiActorObjectGlbSharedMeshWrite OBJECT src/Interchange/GLB/Object3DGlbSharedMeshWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbSharedMeshWrite)
add_dependencies(iiActorObjectGlbSharedMeshWrite iiActorObjectGlbPointAttributeWrite iiActorObjectGlbBuffer iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbSharedMeshWrite>)
add_library(iiActorObjectGlbMeshExtrasWrite OBJECT src/Interchange/GLB/Object3DGlbMeshExtrasWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMeshExtrasWrite)
add_dependencies(iiActorObjectGlbMeshExtrasWrite iiActorObjectGlbPointIdentity iiActorObjectGlbBuffer iiActorObjectSkeletalMotion)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbMeshExtrasWrite>)
add_library(iiActorObjectGlbMeshWrite OBJECT src/Interchange/GLB/Object3DGlbMeshWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMeshWrite)
add_dependencies(iiActorObjectGlbMeshWrite iiActorObjectGlbPrimitivePlan iiActorObjectGlbFbxTangentRoute
    iiActorObjectGlbMaterialPlan iiActorObjectGlbTangentPlan iiActorObjectGlbTangentWrite
    iiActorObjectGlbSharedMeshWrite iiActorObjectGlbMeshExtrasWrite)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbMeshWrite>)

# Point restoration creates a detached plan from topology and decoded sidecar data.
add_library(iiActorObjectGlbPointRestoreCore OBJECT src/Interchange/GLB/Object3DGlbPointRestoreCore.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbPointRestoreCore)
add_dependencies(iiActorObjectGlbPointRestoreCore iiActorObjectGlbPointIdentity iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbPointRestoreCore>)

# Resource-slot reconciliation borrows staged arrays without owning an editor.
add_library(iiActorObjectTemplateSurfaceResources OBJECT src/Geometry/Object3DTemplateSurfaceResources.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceResources)
add_dependencies(iiActorObjectTemplateSurfaceResources iiActorObjectAssetData iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceResources>)

# Material routing depends on a replaceable mapping interface; the native
# adapter connects it to the staged resource mapper.
add_library(iiActorObjectTemplateSurfaceMaterialRouting OBJECT src/Geometry/Object3DTemplateSurfaceMaterialRouting.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceMaterialRouting)
add_dependencies(iiActorObjectTemplateSurfaceMaterialRouting iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceMaterialRouting>)
add_library(iiActorObjectTemplateSurfaceMaterialNative OBJECT src/Geometry/Object3DTemplateSurfaceMaterialNative.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceMaterialNative)
add_dependencies(iiActorObjectTemplateSurfaceMaterialNative iiActorObjectTemplateSurfaceResources)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceMaterialNative>)

# Resource replacement planning borrows data and preserves editor transaction ownership.
add_library(iiActorObjectTemplateSurfaceReplacementResources OBJECT src/Geometry/Object3DTemplateSurfaceReplacementResources.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceReplacementResources)
add_dependencies(iiActorObjectTemplateSurfaceReplacementResources iiActorObjectTemplateSurfaceResources)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceReplacementResources>)

# Resource updates for immutable layer snapshots do not depend on editor sessions.
add_library(iiActorObjectTemplateSurfaceReplacementLayers OBJECT src/Geometry/Object3DTemplateSurfaceReplacementLayers.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceReplacementLayers)
add_dependencies(iiActorObjectTemplateSurfaceReplacementLayers iiActorObjectTemplateSurfaceReplacementResources)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceReplacementLayers>)

# GLB animation planning and channel output borrow data and a skeleton identity.
add_library(iiActorObjectGlbAnimationWrite OBJECT src/Interchange/GLB/Object3DGlbAnimationWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbAnimationWrite)
add_dependencies(iiActorObjectGlbAnimationWrite iiActorObjectGlbBuffer iiActorObjectAssetData iiActorObjectSkeletalMotion)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbAnimationWrite>)

# Project version planning and graph serialization are separate from publication.
add_library(iiActorObjectProjectTemplateAdapter OBJECT src/Project/Object3DProjectTemplateAdapter.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectTemplateAdapter)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectProjectTemplateAdapter>)
add_library(iiActorObjectProjectVersion OBJECT src/Project/Object3DProjectVersion.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectVersion)
add_dependencies(iiActorObjectProjectVersion iiActorObjectAssetData iiActorObjectMeshFoundation iiActorObjectSkeletalMotion)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectProjectVersion>)

add_library(iiActorObjectProjectCodec OBJECT src/Project/Object3DProjectCodec.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectCodec)
add_dependencies(iiActorObjectProjectCodec iiActorObjectProjectVersion iiActorObjectAssetData iiActorObjectMeshFoundation iiActorObjectSkeletalMotion)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectProjectCodec>)

# Saved adjustment reference checks borrow value data, not the editor or engine.
add_library(iiActorObjectProjectAdjustmentValidation OBJECT src/Project/Object3DProjectAdjustmentValidation.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectAdjustmentValidation)
add_dependencies(iiActorObjectProjectAdjustmentValidation iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectProjectAdjustmentValidation>)

# Rig archive construction borrows program values; restoration connects them to
# engine-owned sessions through the existing backend interfaces.
add_library(iiActorObjectProjectRigArchive OBJECT src/Project/Object3DProjectRigArchive.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectRigArchive)
add_dependencies(iiActorObjectProjectRigArchive iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectProjectRigArchive>)
add_library(iiActorObjectProjectRigRestore OBJECT src/Project/Object3DProjectRigRestore.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectRigRestore)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectProjectRigRestore>)

# Capture and restore are separate engine-aware project adapters. Their value
# validations and serialized graph belong to independent lower-level modules.
add_library(iiActorObjectProjectCapture OBJECT src/Project/Object3DProjectCapture.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectCapture)
add_dependencies(iiActorObjectProjectCapture iiActorObjectProjectRigArchive)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectProjectCapture>)
add_library(iiActorObjectProjectRestore OBJECT src/Project/Object3DProjectRestore.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectRestore)
add_dependencies(iiActorObjectProjectRestore iiActorObjectProjectHistorySelection iiActorObjectProjectRigRestore)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectProjectRestore>)

# Resource export only borrows material/image data and FBX node containers.
add_library(iiActorObjectFbxResourceWrite OBJECT src/Interchange/FBX/Object3DFbxResourceWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxResourceWrite)
add_dependencies(iiActorObjectFbxResourceWrite iiActorObjectFbxBinary iiActorObjectFbxSceneNodes iiActorObjectAssetData)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxResourceWrite>)

# FBX DOM value and topology metadata codecs do not own scene or engine objects.
add_library(iiActorObjectFbxMetadata OBJECT
    src/Interchange/FBX/Object3DFbxMetadata.cpp src/Interchange/FBX/Object3DFbxImportFields.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMetadata)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxMetadata>)

add_library(iiActorObjectFbxExternalIdentityRead OBJECT src/Interchange/FBX/Object3DFbxExternalIdentityRead.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxExternalIdentityRead)
add_dependencies(iiActorObjectFbxExternalIdentityRead iiActorObjectFbxMetadata iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxExternalIdentityRead>)
add_library(iiActorObjectFbxExternalIdentityWrite OBJECT src/Interchange/FBX/Object3DFbxExternalIdentityWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxExternalIdentityWrite)
add_dependencies(iiActorObjectFbxExternalIdentityWrite iiActorObjectFbxBinary iiActorObjectFbxSceneNodes iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxExternalIdentityWrite>)
add_library(iiActorObjectFbxSceneMetadata OBJECT src/Interchange/FBX/Object3DFbxSceneMetadata.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSceneMetadata)
add_dependencies(iiActorObjectFbxSceneMetadata iiActorObjectFbxMetadata iiActorObjectAssetData)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxSceneMetadata>)
add_library(iiActorObjectFbxSceneMetadataWrite OBJECT src/Interchange/FBX/Object3DFbxSceneMetadataWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSceneMetadataWrite)
add_dependencies(iiActorObjectFbxSceneMetadataWrite iiActorObjectFbxBinary iiActorObjectFbxSceneNodes)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxSceneMetadataWrite>)

add_library(iiActorObjectFbxProgressiveMetadata OBJECT src/Interchange/FBX/Object3DFbxProgressiveMetadata.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxProgressiveMetadata)
add_dependencies(iiActorObjectFbxProgressiveMetadata iiActorObjectFbxMetadata)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxProgressiveMetadata>)

# FBX tangent source restoration borrows topology and parsed layer data.
add_library(iiActorObjectFbxTangentRead OBJECT src/Interchange/FBX/Object3DFbxTangentRead.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxTangentRead)
add_dependencies(iiActorObjectFbxTangentRead iiActorObjectFbxMetadata iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxTangentRead>)

add_library(iiActorObjectFbxTopology OBJECT
    src/Interchange/FBX/Object3DFbxTopologyRead.cpp src/Interchange/FBX/Object3DFbxTopologyWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxTopology)
add_dependencies(iiActorObjectFbxTopology iiActorObjectFbxBinary iiActorObjectFbxMetadata iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxTopology>)

# FBX mesh node construction accepts tangent data without owning its producer.
add_library(iiActorObjectFbxGeometryWrite OBJECT src/Interchange/FBX/Object3DFbxGeometryWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxGeometryWrite)
add_dependencies(iiActorObjectFbxGeometryWrite iiActorObjectFbxBinary iiActorObjectFbxSceneNodes iiActorObjectFbxTopology iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxGeometryWrite>)

# FBX image and material decoding borrows the scene and a host resolver interface.
add_library(iiActorObjectFbxResources OBJECT src/Interchange/FBX/Object3DFbxResources.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxResources)
add_dependencies(iiActorObjectFbxResources iiActorObjectFbxMetadata iiActorObjectAssetData)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxResources>)

# FBX skeleton reconstruction is independent of engine publication and editing.
add_library(iiActorObjectFbxSkeleton OBJECT src/Interchange/FBX/Object3DFbxSkeleton.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSkeleton)
add_dependencies(iiActorObjectFbxSkeleton iiActorObjectFbxMetadata iiActorObjectAssetData)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxSkeleton>)

# FBX skin weights and blend profiles use mesh/bone data, not engine objects.
add_library(iiActorObjectFbxSkin OBJECT src/Interchange/FBX/Object3DFbxSkin.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSkin)
add_dependencies(iiActorObjectFbxSkin iiActorObjectFbxMetadata iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxSkin>)

# Reconstruct FBX mesh points, UVs and polygon layouts without an engine.
add_library(iiActorObjectFbxGeometryRead OBJECT src/Interchange/FBX/Object3DFbxGeometryRead.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxGeometryRead)
add_dependencies(iiActorObjectFbxGeometryRead iiActorObjectFbxSkin iiActorObjectFbxTopology iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxGeometryRead>)

# FBX skin writing borrows point rows, bone matrices and the output node lists.
add_library(iiActorObjectFbxSkinWrite OBJECT src/Interchange/FBX/Object3DFbxSkinWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSkinWrite)
add_dependencies(iiActorObjectFbxSkinWrite iiActorObjectFbxBinary iiActorObjectFbxSceneNodes iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxSkinWrite>)

# Skeleton and bind-pose nodes borrow skeletal values and staging containers.
add_library(iiActorObjectFbxSkeletonWrite OBJECT src/Interchange/FBX/Object3DFbxSkeletonWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSkeletonWrite)
add_dependencies(iiActorObjectFbxSkeletonWrite iiActorObjectFbxBinary iiActorObjectFbxSceneNodes
    iiActorObjectFbxSceneMetadataWrite iiActorObjectFbxPoseWrite)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxSkeletonWrite>)

# FBX morph shape conversion borrows source topology and format data only.
add_library(iiActorObjectFbxMorphShape OBJECT src/Interchange/FBX/Object3DFbxMorphShape.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMorphShape)
add_dependencies(iiActorObjectFbxMorphShape iiActorObjectFbxMetadata iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxMorphShape>)

# Accumulate and convert FBX morph channels without publishing an engine asset.
add_library(iiActorObjectFbxMorphChannelRead OBJECT src/Interchange/FBX/Object3DFbxMorphChannelRead.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMorphChannelRead)
add_dependencies(iiActorObjectFbxMorphChannelRead iiActorObjectFbxProgressiveMetadata iiActorObjectFbxMorphShape iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxMorphChannelRead>)

# Traverse and stage FBX meshes using borrowed scene data, without publishing an asset.
add_library(iiActorObjectFbxMeshSceneRead OBJECT src/Interchange/FBX/Object3DFbxMeshSceneRead.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMeshSceneRead)
add_dependencies(iiActorObjectFbxMeshSceneRead iiActorObjectFbxGeometryRead iiActorObjectFbxMorphChannelRead
    iiActorObjectFbxExternalIdentityRead iiActorObjectFbxTangentRead)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxMeshSceneRead>)

# FBX morph projection and shape node writing have no asset or engine ownership.
add_library(iiActorObjectFbxMorphWrite OBJECT src/Interchange/FBX/Object3DFbxMorphWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMorphWrite)
add_dependencies(iiActorObjectFbxMorphWrite iiActorObjectFbxBinary iiActorObjectFbxSceneNodes iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxMorphWrite>)

add_library(iiActorObjectMorphChannels OBJECT src/Rig/Object3DMorphChannels.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectMorphChannels)
add_dependencies(iiActorObjectMorphChannels iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectMorphChannels>)
add_library(iiActorObjectFbxMorphChannelsWrite OBJECT src/Interchange/FBX/Object3DFbxMorphChannelsWrite.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMorphChannelsWrite)
add_dependencies(iiActorObjectFbxMorphChannelsWrite iiActorObjectMorphChannels iiActorObjectFbxMorphWrite)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxMorphChannelsWrite>)
add_library(iiActorObjectFbxSharedGeometry OBJECT src/Interchange/FBX/Object3DFbxSharedGeometry.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSharedGeometry)
add_dependencies(iiActorObjectFbxSharedGeometry iiActorObjectFbxBinary iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxSharedGeometry>)
add_library(iiActorObjectFbxGeometryRegistry OBJECT src/Interchange/FBX/Object3DFbxGeometryRegistry.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxGeometryRegistry)
add_dependencies(iiActorObjectFbxGeometryRegistry iiActorObjectFbxSharedGeometry iiActorObjectFbxBinary)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectFbxGeometryRegistry>)

# Skeletal deformation uses mesh data and poses, not scene component ownership.
add_library(iiActorObjectDeformation OBJECT
    src/Rig/SkeletalMesh.cpp src/Rig/DualQuaternionSkinning.cpp
    src/Rig/SkinningPalette.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectDeformation)
add_dependencies(iiActorObjectDeformation iiActorObjectMeshFoundation iiActorObjectSkeletalMotion)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectDeformation>)

find_package(iiJson 0.1 CONFIG REQUIRED)
target_link_libraries(iiActorObject PRIVATE iiJson::iiJson)
# GLB container and accessor decoding depends only on JSON and standard data.
add_library(iiActorObjectGlbDocument OBJECT src/Interchange/GLB/GlbDocument.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbDocument)
target_link_libraries(iiActorObjectGlbDocument PRIVATE iiJson::iiJson)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbDocument>)
# JSON field policy and primitive attribute identification do not own an engine.
add_library(iiActorObjectGlbJson OBJECT src/Interchange/GLB/Object3DGlbJson.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbJson)
target_link_libraries(iiActorObjectGlbJson PRIVATE iiJson::iiJson)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbJson>)
add_library(iiActorObjectGlbTangentNotes OBJECT src/Interchange/GLB/Object3DGlbTangentNotes.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTangentNotes)
target_link_libraries(iiActorObjectGlbTangentNotes PRIVATE iiJson::iiJson)
add_dependencies(iiActorObjectGlbTangentNotes iiActorObjectGlbJson)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbTangentNotes>)
add_library(iiActorObjectGlbTransform OBJECT src/Interchange/GLB/Object3DGlbTransform.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTransform)
target_link_libraries(iiActorObjectGlbTransform PRIVATE iiJson::iiJson)
add_dependencies(iiActorObjectGlbTransform iiActorObjectGlbJson iiActorObjectFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbTransform>)
add_library(iiActorObjectGlbAccessorRead OBJECT src/Interchange/GLB/Object3DGlbAccessorRead.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbAccessorRead)
target_link_libraries(iiActorObjectGlbAccessorRead PRIVATE iiJson::iiJson)
add_dependencies(iiActorObjectGlbAccessorRead iiActorObjectGlbDocument iiActorObjectGlbJson)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbAccessorRead>)
add_library(iiActorObjectGlbMeshAttributes OBJECT src/Interchange/GLB/Object3DGlbMeshAttributes.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMeshAttributes)
target_link_libraries(iiActorObjectGlbMeshAttributes PRIVATE iiJson::iiJson)
add_dependencies(iiActorObjectGlbMeshAttributes iiActorObjectGlbJson)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbMeshAttributes>)
add_library(iiActorObjectGlbMeshBundle OBJECT src/Interchange/GLB/Object3DGlbMeshBundle.cpp)
iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMeshBundle)
target_link_libraries(iiActorObjectGlbMeshBundle PRIVATE iiJson::iiJson)
add_dependencies(iiActorObjectGlbMeshBundle iiActorObjectGlbAccessorRead iiActorObjectGlbTransform iiActorObjectGlbMeshAttributes iiActorObjectMeshFoundation)
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectGlbMeshBundle>)
if(IIACTOROBJECT_WITH_NSCHARACTER)
    # The character layer is compiled separately. It consumes core interfaces;
    # the final library contains both without a reverse dependency from core.
    add_library(iiActorObjectNSCharacter OBJECT
        src/NSCharacter/CharacterAsset.cpp
        src/NSCharacter/HumanoidGenerator.cpp
        src/NSCharacter/CharacterEditor.cpp
        src/NSCharacter/CharacterInterchange.cpp
        src/NSCharacter/HumanPoseDirections.cpp
        src/NSCharacter/HumanPosePlanner.cpp
        src/NSCharacter/HumanAutomaticCorrectives.cpp
        src/NSCharacter/HumanPoseRetarget.cpp
        src/NSCharacter/HumanSkin.cpp
        src/NSCharacter/HumanJointNaming.cpp
        src/NSCharacter/RigSamples.cpp
        src/NSCharacter/HumanoidCharacterization.cpp
        src/NSCharacter/CharacterTemplateCore.cpp
        src/NSCharacter/HumanAccessories.cpp
        src/NSCharacter/HumanAccessoryCatalogInternal.cpp
        src/NSCharacter/MakeHumanResourceReader.cpp
        src/NSCharacter/MakeHumanMorphology.cpp
        src/NSCharacter/MakeHumanExpressions.cpp
        src/NSCharacter/MakeHumanAttachments.cpp
        src/NSCharacter/MakeHumanRigReader.cpp
        src/NSCharacter/MakeHumanTemplate.cpp
        src/NSCharacter/HumanMorphology.cpp
        src/NSCharacter/DnaCharacter.cpp)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectNSCharacter)
    target_link_libraries(iiActorObjectNSCharacter PRIVATE iiJson::iiJson)
    target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectNSCharacter>)
endif()
if(MSVC)
    set_source_files_properties(src/third_party/ufbx/ufbx.cpp src/third_party/mikktspace/mikktspace.cpp PROPERTIES COMPILE_OPTIONS /w)
else()
    set_source_files_properties(src/third_party/ufbx/ufbx.cpp src/third_party/mikktspace/mikktspace.cpp PROPERTIES COMPILE_OPTIONS -w)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_source_files_properties(src/Interchange/FBX/Object3DFbxImport.cpp src/Interchange/FBX/Object3DFbxMetadata.cpp src/Interchange/FBX/Object3DFbxProgressiveMetadata.cpp PROPERTIES COMPILE_OPTIONS "-Wno-gnu-anonymous-struct;-Wno-nested-anon-types")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_source_files_properties(src/Interchange/FBX/Object3DFbxImport.cpp src/Interchange/FBX/Object3DFbxMetadata.cpp src/Interchange/FBX/Object3DFbxProgressiveMetadata.cpp PROPERTIES COMPILE_OPTIONS -Wno-pedantic)
    endif()
endif()
add_library(iiActorObject::iiActorObject ALIAS iiActorObject)
if(IIACTOROBJECT_WITH_DNA)
    include(cmake/OpenRigLogic.cmake)
endif()
target_compile_features(iiActorObject PUBLIC cxx_std_23)
generate_export_header(iiActorObject BASE_NAME IIACTOROBJECT
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/Export.h")
configure_file(cmake/iiActorObjectFeatures.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/Features.h")
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(iiActorObject PUBLIC IIACTOROBJECT_STATIC_DEFINE)
endif()
if(MSVC)
    target_compile_options(iiActorObject PUBLIC /Zc:__cplusplus PRIVATE /W4 /permissive-)
else()
    target_compile_options(iiActorObject PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_include_directories(iiActorObject PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
foreach(_dir IN LISTS _iiActorObject_source_dirs)
    target_include_directories(iiActorObject PUBLIC "$<BUILD_INTERFACE:${_dir}>")
endforeach()
set_target_properties(iiActorObject PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0.2
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

if(IIACTOROBJECT_WITH_VISUALCODEC)
    find_package(iiVisualCodec 0.2 CONFIG REQUIRED COMPONENTS Image)
    add_library(iiActorObjectVisualCodec STATIC src/Interchange/FBX/FbxVisualImageResolver.cpp)
    add_library(iiActorObject::VisualCodec ALIAS iiActorObjectVisualCodec)
    target_compile_features(iiActorObjectVisualCodec PUBLIC cxx_std_23)
    target_link_libraries(iiActorObjectVisualCodec PUBLIC iiActorObject::iiActorObject PRIVATE iiVisualCodec::Image)
    set_target_properties(iiActorObjectVisualCodec PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON EXPORT_NAME VisualCodec)
    install(TARGETS iiActorObjectVisualCodec EXPORT iiActorObjectVisualCodecTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES src/Interchange/FBX/FbxVisualImageResolver.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject)
    if(BUILD_TESTING)
        add_executable(iiActorObjectFbxVisualImageTests tests/FbxVisualImageResolverTest.cpp)
        target_link_libraries(iiActorObjectFbxVisualImageTests PRIVATE iiActorObject::VisualCodec iiVisualCodec::Image)
        set_target_properties(iiActorObjectFbxVisualImageTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case resolution limits passthrough embedded)
            add_test(NAME iiActorObject.fbx_visual_image.${_case} COMMAND iiActorObjectFbxVisualImageTests ${_case}
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/external-textures-ascii.fbx"
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png"
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/normal-map-ascii.fbx")
        endforeach()
    endif()
endif()

option(IIACTOROBJECT_BUILD_TOOLS "Build installed agent-facing SDK tools" ON)
option(IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION "Enable optional application execution checks when explicitly requested" OFF)
if(IIACTOROBJECT_BUILD_TOOLS)
    add_executable(iiActorObjectRigSamples example/RigMorphSamples.cpp)
    target_link_libraries(iiActorObjectRigSamples PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectRigSamples PROPERTIES CXX_EXTENSIONS OFF)
    if(APPLE)
        set_target_properties(iiActorObjectRigSamples PROPERTIES INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
    elseif(UNIX)
        set_target_properties(iiActorObjectRigSamples PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()
    install(TARGETS iiActorObjectRigSamples RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
option(IIACTOROBJECT_BUILD_EXAMPLE "Build the standalone character authoring example" ON)
if(IIACTOROBJECT_BUILD_EXAMPLE)
    add_executable(iiActorObjectPoseSnapshot example/PoseSnapshot.cpp)
    target_link_libraries(iiActorObjectPoseSnapshot PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectPoseSnapshot PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(iiActorObjectCore3D example/Core3DObject.cpp)
    target_link_libraries(iiActorObjectCore3D PRIVATE iiActorObject::iiActorObject)
endif()
if(IIACTOROBJECT_BUILD_EXAMPLE AND IIACTOROBJECT_WITH_NSCHARACTER)
    add_executable(iiActorObjectCharacterize example/HumanoidCharacterization.cpp)
    target_link_libraries(iiActorObjectCharacterize PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectCharacterize PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(iiActorObjectTemplateCharacter example/TemplateCharacter.cpp)
    target_link_libraries(iiActorObjectTemplateCharacter PRIVATE iiActorObject::iiActorObject)
    add_executable(iiActorObjectCharacterAuthoring example/CharacterAuthoring.cpp)
    target_link_libraries(iiActorObjectCharacterAuthoring PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectCharacterAuthoring PROPERTIES CXX_EXTENSIONS OFF)
    if(IIACTOROBJECT_WITH_DNA)
        add_executable(iiActorObjectDnaCharacter example/DnaCharacter.cpp)
        target_link_libraries(iiActorObjectDnaCharacter PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectDnaCharacter PROPERTIES CXX_EXTENSIONS OFF)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(iiActorObjectGlbAnimationImportTests tests/GlbAnimationSourceTest.cpp)
    target_link_libraries(iiActorObjectGlbAnimationImportTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectGlbAnimationImportTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case interpolation validation import)
        add_test(NAME iiActorObject.glb_animation_source.${_case} COMMAND iiActorObjectGlbAnimationImportTests ${_case})
    endforeach()
    add_executable(iiActorObjectAnimationApplyTests tests/Object3DAnimationApplyTest.cpp)
    target_link_libraries(iiActorObjectAnimationApplyTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectAnimationApplyTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case apply validation bind static)
        add_test(NAME iiActorObject.animation_apply.${_case} COMMAND iiActorObjectAnimationApplyTests ${_case})
    endforeach()
    add_executable(iiActorObjectGlbAnimationBakeTests tests/GlbAnimationBakeTest.cpp)
    target_link_libraries(iiActorObjectGlbAnimationBakeTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectGlbAnimationBakeTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case linear cubic step validation identity static)
        add_test(NAME iiActorObject.glb_animation_bake.${_case} COMMAND iiActorObjectGlbAnimationBakeTests ${_case})
    endforeach()
    if(IIACTOROBJECT_WITH_NSCHARACTER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman/base.obj")
        add_test(NAME iiActorObject.glb_animation_bake.human COMMAND iiActorObjectGlbAnimationBakeTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
    endif()
    add_executable(iiActorObjectGlbImportTests tests/Object3DGlbImportTest.cpp)
    target_link_libraries(iiActorObjectGlbImportTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectGlbImportTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case roundtrip validation multiskin mixed_attributes surface)
        add_test(NAME iiActorObject.glb_import.${_case} COMMAND iiActorObjectGlbImportTests ${_case})
    endforeach()
    add_executable(iiActorObjectGlbDocumentTests tests/GlbDocumentTest.cpp)
    target_link_libraries(iiActorObjectGlbDocumentTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectGlbDocumentTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case valid invalid mutations)
        add_test(NAME iiActorObject.glb_document.${_case} COMMAND iiActorObjectGlbDocumentTests ${_case})
    endforeach()
    add_executable(iiActorObjectGlbDocumentIsolatedTests tests/GlbDocumentTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbDocument>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbDocumentIsolatedTests)
    target_link_libraries(iiActorObjectGlbDocumentIsolatedTests PRIVATE iiJson::iiJson)
    foreach(_case valid invalid mutations)
        add_test(NAME iiActorObject.architecture.glb_document_isolated_${_case} COMMAND iiActorObjectGlbDocumentIsolatedTests ${_case})
    endforeach()
    add_executable(iiActorObjectGlbImportOptionsTests tests/GlbImportOptionsHeaderTest.cpp tests/GlbImageResolverHeaderTest.cpp)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbImportOptionsTests)
    add_test(NAME iiActorObject.architecture.glb_import_options_isolated COMMAND iiActorObjectGlbImportOptionsTests)
    add_executable(iiActorObjectGlbAccessorReadTests tests/GlbAccessorReadTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbAccessorRead> $<TARGET_OBJECTS:iiActorObjectGlbDocument>
        $<TARGET_OBJECTS:iiActorObjectGlbJson>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbAccessorReadTests)
    target_link_libraries(iiActorObjectGlbAccessorReadTests PRIVATE iiJson::iiJson)
    add_test(NAME iiActorObject.architecture.glb_accessor_read_isolated COMMAND iiActorObjectGlbAccessorReadTests)
    add_library(iiActorObjectGlbUfbxTestSupport OBJECT src/third_party/ufbx/ufbx.cpp)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbUfbxTestSupport)
    add_executable(iiActorObjectGlbTransformTests tests/GlbTransformTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbUfbxTestSupport>
        $<TARGET_OBJECTS:iiActorObjectGlbTransform> $<TARGET_OBJECTS:iiActorObjectGlbJson>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTransformTests)
    target_link_libraries(iiActorObjectGlbTransformTests PRIVATE iiJson::iiJson)
    add_test(NAME iiActorObject.architecture.glb_transform_isolated COMMAND iiActorObjectGlbTransformTests)
    add_executable(iiActorObjectGlbMeshBundleTests tests/GlbMeshBundleTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbUfbxTestSupport>
        $<TARGET_OBJECTS:iiActorObjectGlbMeshBundle> $<TARGET_OBJECTS:iiActorObjectGlbAccessorRead>
        $<TARGET_OBJECTS:iiActorObjectGlbTransform> $<TARGET_OBJECTS:iiActorObjectGlbDocument>
        $<TARGET_OBJECTS:iiActorObjectGlbJson> $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMeshBundleTests)
    target_link_libraries(iiActorObjectGlbMeshBundleTests PRIVATE iiJson::iiJson)
    add_test(NAME iiActorObject.architecture.glb_mesh_bundle_isolated COMMAND iiActorObjectGlbMeshBundleTests)
    add_executable(iiActorObjectGlbPrimitiveTopologyTests tests/GlbPrimitiveTopologyTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbPrimitiveTopology>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbPrimitiveTopologyTests)
    add_test(NAME iiActorObject.architecture.glb_primitive_topology_isolated COMMAND iiActorObjectGlbPrimitiveTopologyTests)
    add_executable(iiActorObjectGlbTangentNotesTests tests/GlbTangentNotesTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbTangentNotes> $<TARGET_OBJECTS:iiActorObjectGlbJson>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTangentNotesTests)
    target_link_libraries(iiActorObjectGlbTangentNotesTests PRIVATE iiJson::iiJson)
    add_test(NAME iiActorObject.architecture.glb_tangent_notes_isolated COMMAND iiActorObjectGlbTangentNotesTests)
    add_executable(iiActorObjectGlbBundleIndexTests tests/GlbBundleIndexTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbBundleIndex>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbBundleIndexTests)
    add_test(NAME iiActorObject.architecture.glb_bundle_index_isolated COMMAND iiActorObjectGlbBundleIndexTests)
    add_executable(iiActorObjectGlbTangentSnapshotTests tests/GlbTangentSnapshotTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbTangentSnapshot> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTangentSnapshotTests)
    add_test(NAME iiActorObject.architecture.glb_tangent_snapshot_isolated COMMAND iiActorObjectGlbTangentSnapshotTests)
    add_executable(iiActorObjectRigSnapshotTests tests/Object3DRigSnapshotTest.cpp)
    target_link_libraries(iiActorObjectRigSnapshotTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectRigSnapshotTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case controls partial dual_quaternion failures budgets lifetime)
        add_test(NAME iiActorObject.rig_snapshot.${_case} COMMAND iiActorObjectRigSnapshotTests ${_case})
    endforeach()
    add_executable(iiActorObjectRigSamplesTests tests/Object3DRigSamplesTest.cpp)
    target_link_libraries(iiActorObjectRigSamplesTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectRigSamplesTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case endpoints partial failures lifetime text_budget nonlinear)
        add_test(NAME iiActorObject.rig_samples.${_case} COMMAND iiActorObjectRigSamplesTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.rig_samples.formats COMMAND iiActorObjectRigSamplesTests formats "${CMAKE_CURRENT_BINARY_DIR}/rig-samples")
    set_tests_properties(iiActorObject.rig_samples.formats PROPERTIES FIXTURES_SETUP iiActorObject_rig_samples_formats)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME iiActorObject.architecture.source_layout COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifySourceLayout.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME iiActorObject.architecture.responsibility_map COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyResponsibilityMap.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME iiActorObject.architecture.responsibility_boundaries COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyResponsibilityBoundaries.py"
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME iiActorObject.glb_animation_bake.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyGlbAnimationBake.py" "$<TARGET_FILE:iiActorObjectGlbAnimationBakeTests>"
        "$<TARGET_FILE:iiActorObjectFbxAnimationUfbxTests>")
    add_test(NAME iiActorObject.glb_import.multiskin_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyGlbMultiSkin.py" "$<TARGET_FILE:iiActorObjectGlbImportTests>")
    add_test(NAME iiActorObject.glb_import.mixed_attributes_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyGlbMixedAttributes.py" "$<TARGET_FILE:iiActorObjectGlbImportTests>")
    add_test(NAME iiActorObject.glb_import.surface_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyMaterialSurface.py" "$<TARGET_FILE:iiActorObjectGlbImportTests>")
    add_executable(iiActorObjectGlbAuthoredTangentsTests tests/GlbAuthoredTangentsTest.cpp)
    target_link_libraries(iiActorObjectGlbAuthoredTangentsTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectGlbAuthoredTangentsTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.glb_import.authored_tangents COMMAND iiActorObjectGlbAuthoredTangentsTests)
    add_test(NAME iiActorObject.glb_import.authored_tangents_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyGlbAuthoredTangents.py" "$<TARGET_FILE:iiActorObjectGlbAuthoredTangentsTests>")
    add_executable(iiActorObjectMeshTangentGlbTests tests/MeshTangentGlbTest.cpp)
    target_link_libraries(iiActorObjectMeshTangentGlbTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectMeshTangentGlbTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.mesh_tangents.glb_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyMorphTangentsGlb.py" "$<TARGET_FILE:iiActorObjectMeshTangentGlbTests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png")
    add_executable(iiActorObjectFbxTangentTests tests/FbxTangentsTest.cpp)
    target_link_libraries(iiActorObjectFbxTangentTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectFbxTangentTests PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(iiActorObjectFbxAuthoredTangentTests tests/FbxAuthoredTangentTest.cpp)
    target_link_libraries(iiActorObjectFbxAuthoredTangentTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectFbxAuthoredTangentTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.fbx_tangents.authored_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyFbxAuthoredTangents.py"
        "$<TARGET_FILE:iiActorObjectFbxAuthoredTangentTests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/normal-map-ascii.fbx")
    add_executable(iiActorObjectFbxTangentUfbxTests tests/FbxTangentUfbxTest.cpp src/third_party/ufbx/ufbx.cpp)
    target_compile_features(iiActorObjectFbxTangentUfbxTests PRIVATE cxx_std_23)
    set_target_properties(iiActorObjectFbxTangentUfbxTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.fbx_tangents.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyFbxTangents.py" "$<TARGET_FILE:iiActorObjectFbxTangentTests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png" "$<TARGET_FILE:iiActorObjectFbxTangentUfbxTests>")
    foreach(_mode warning warning_conflict warning_morph warning_unknown warning_foreign)
        add_test(NAME iiActorObject.fbx_tangents.${_mode} COMMAND iiActorObjectFbxTangentTests ${_mode}
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png")
    endforeach()
    if(IIACTOROBJECT_WITH_VISUALCODEC)
        add_test(NAME iiActorObject.fbx_visual_image.independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyResolvedImageFormats.py"
            "$<TARGET_FILE:iiActorObjectFbxVisualImageTests>"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/external-textures-ascii.fbx"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/normal-map-ascii.fbx")
    endif()
    add_test(NAME iiActorObject.rig_samples.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyRigSamplesFormat.py" "${CMAKE_CURRENT_BINARY_DIR}/rig-samples"
        "$<TARGET_FILE:iiActorObjectRigSamplesTests>")
    set_tests_properties(iiActorObject.rig_samples.independent PROPERTIES FIXTURES_REQUIRED iiActorObject_rig_samples_formats)
    if(NOT IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION)
        add_test(NAME iiActorObject.execution_policy.registration COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyExecutionPolicy.py" "${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME iiActorObject.execution_policy.validation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/ExecutionRegistrationPolicyTest.py")
    endif()
    if(IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION AND TARGET iiActorObjectRigSamples)
        add_test(NAME iiActorObject.rig_samples.project_cli COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyRigSamplesProjectCli.py" "$<TARGET_FILE:iiActorObjectRigSamples>"
            "$<TARGET_FILE:iiActorObjectRigSamplesTests>" "${CMAKE_CURRENT_BINARY_DIR}/rig-samples-project-cli")
    endif()
    add_executable(iiActorObjectDeformationSnapshotTests tests/Object3DDeformationSnapshotTest.cpp)
    target_link_libraries(iiActorObjectDeformationSnapshotTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectDeformationSnapshotTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case render topology generated_normals reflected multiple zero dual_quaternion dual_quaternion_failures reference static failures cancellation overflow roundtrip lifetime)
        add_test(NAME iiActorObject.deformation_snapshot.${_case} COMMAND iiActorObjectDeformationSnapshotTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.deformation_snapshot.linear_glb_fixture COMMAND iiActorObjectDeformationSnapshotTests
        linear_glb_fixture "${CMAKE_CURRENT_BINARY_DIR}/linearized-glb-fixture")
    set_tests_properties(iiActorObject.deformation_snapshot.linear_glb_fixture PROPERTIES FIXTURES_SETUP iiActorObject_linearized_glb)
    add_test(NAME iiActorObject.deformation_snapshot.linear_glb_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyLinearizedSnapshotGlb.py" "${CMAKE_CURRENT_BINARY_DIR}/linearized-glb-fixture")
    set_tests_properties(iiActorObject.deformation_snapshot.linear_glb_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_linearized_glb)
    add_executable(iiActorObjectPoseSnapshotTests tests/Object3DPoseSnapshotTest.cpp)
    target_link_libraries(iiActorObjectPoseSnapshotTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectPoseSnapshotTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case render topology generated_normals many_weights reflected cancellation failures roundtrip lifetime wrappers static multiple frames)
        add_test(NAME iiActorObject.pose_snapshot.${_case} COMMAND iiActorObjectPoseSnapshotTests ${_case})
    endforeach()
    add_executable(iiActorObjectPoseAimingTests tests/PoseAimingTest.cpp)
    target_link_libraries(iiActorObjectPoseAimingTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectPoseAimingTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case direction scaled reflection opposite large_origin dynamic_bind failures group)
        add_test(NAME iiActorObject.pose_aim.${_case} COMMAND iiActorObjectPoseAimingTests ${_case})
    endforeach()
    add_executable(iiActorObjectPoseRetargetTests tests/PoseRetargetTest.cpp)
    target_link_libraries(iiActorObjectPoseRetargetTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectPoseRetargetTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case rotations basis_translation animation dynamic_bind validation)
        add_test(NAME iiActorObject.pose_retarget.${_case} COMMAND iiActorObjectPoseRetargetTests ${_case})
    endforeach()
    add_executable(iiActorObjectAnimationRetargetTests tests/AnimationRetargetTest.cpp)
    target_link_libraries(iiActorObjectAnimationRetargetTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectAnimationRetargetTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case samples curves dynamic_bind validation)
        add_test(NAME iiActorObject.animation_retarget.${_case} COMMAND iiActorObjectAnimationRetargetTests ${_case})
    endforeach()
    add_executable(iiActorObjectGlbAnimationTests tests/Object3DGlbAnimationTest.cpp)
    target_link_libraries(iiActorObjectGlbAnimationTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectGlbAnimationTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.glb_animation.validation COMMAND iiActorObjectGlbAnimationTests validation)
    add_test(NAME iiActorObject.glb_animation.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyGlbAnimation.py" "$<TARGET_FILE:iiActorObjectGlbAnimationTests>")
    add_executable(iiActorObjectFbxAnimationTests tests/Object3DFbxAnimationTest.cpp)
    target_link_libraries(iiActorObjectFbxAnimationTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectFbxAnimationTests PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(iiActorObjectFbxAnimationUfbxTests tests/FbxAnimationUfbxTest.cpp src/third_party/ufbx/ufbx.cpp)
    target_compile_features(iiActorObjectFbxAnimationUfbxTests PRIVATE cxx_std_23)
    set_target_properties(iiActorObjectFbxAnimationUfbxTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.fbx_animation.validation COMMAND iiActorObjectFbxAnimationTests validation)
    add_test(NAME iiActorObject.fbx_animation.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyFbxAnimation.py" "$<TARGET_FILE:iiActorObjectFbxAnimationTests>"
        "$<TARGET_FILE:iiActorObjectFbxAnimationUfbxTests>")
    add_executable(iiActorObjectFbxAnimationImportTests tests/Object3DFbxAnimationImportTest.cpp)
    target_link_libraries(iiActorObjectFbxAnimationImportTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectFbxAnimationImportTests PROPERTIES CXX_EXTENSIONS OFF)
    # The FBX writer must be usable without the engine or format importer.
    add_executable(iiActorObjectFbxBinaryTests tests/FbxBinaryWriterTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxBinary>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxBinaryTests)
    add_test(NAME iiActorObject.architecture.fbx_binary_isolated COMMAND iiActorObjectFbxBinaryTests)
    add_executable(iiActorObjectFbxSceneNodesTests tests/FbxSceneNodesTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes> $<TARGET_OBJECTS:iiActorObjectFbxBinary>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSceneNodesTests)
    add_test(NAME iiActorObject.architecture.fbx_scene_nodes_isolated COMMAND iiActorObjectFbxSceneNodesTests)
    add_executable(iiActorObjectFbxAnimationNodesTests tests/FbxAnimationNodesTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxAnimationNodes> $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes>
        $<TARGET_OBJECTS:iiActorObjectFbxBinary> $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxAnimationNodesTests)
    add_test(NAME iiActorObject.architecture.fbx_animation_nodes_isolated COMMAND iiActorObjectFbxAnimationNodesTests)
    add_executable(iiActorObjectFbxTopologyMetadataTests tests/FbxTopologyMetadataTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxBinary>
        $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectFbxTopology>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxTopologyMetadataTests)
    add_test(NAME iiActorObject.architecture.fbx_topology_metadata_isolated COMMAND iiActorObjectFbxTopologyMetadataTests)
    add_executable(iiActorObjectFbxGeometryWriteTests tests/FbxGeometryWriteTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxGeometryWrite>
        $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes> $<TARGET_OBJECTS:iiActorObjectFbxBinary>
        $<TARGET_OBJECTS:iiActorObjectFbxTopology> $<TARGET_OBJECTS:iiActorObjectFbxMetadata>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxGeometryWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_geometry_write_isolated COMMAND iiActorObjectFbxGeometryWriteTests)
    add_executable(iiActorObjectFbxPoseWriteTests tests/FbxPoseWriteTest.cpp src/Core/Transform.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxPoseWrite> $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes>
        $<TARGET_OBJECTS:iiActorObjectFbxBinary>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxPoseWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_pose_write_isolated COMMAND iiActorObjectFbxPoseWriteTests)
    add_executable(iiActorObjectFbxMeshSceneWriteTests tests/FbxMeshSceneWriteTest.cpp src/Core/Transform.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxMeshSceneWrite> $<TARGET_OBJECTS:iiActorObjectFbxPoseWrite>
        $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes> $<TARGET_OBJECTS:iiActorObjectFbxBinary>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMeshSceneWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_mesh_scene_write_isolated COMMAND iiActorObjectFbxMeshSceneWriteTests)
    add_executable(iiActorObjectTemplateSurfaceTopologyTests tests/TemplateSurfaceTopologyTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceTopology>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceTopologyTests)
    add_test(NAME iiActorObject.architecture.template_surface_topology_isolated COMMAND iiActorObjectTemplateSurfaceTopologyTests)
    add_executable(iiActorObjectTemplateSurfaceMeshesTests tests/TemplateSurfaceMeshesTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceMeshes>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceMeshesTests)
    add_test(NAME iiActorObject.architecture.template_surface_meshes_isolated COMMAND iiActorObjectTemplateSurfaceMeshesTests)
    add_executable(iiActorObjectTemplateSurfaceSlotPlanTests tests/TemplateSurfaceSlotPlanTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceSlots>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceSlotPlanTests)
    add_test(NAME iiActorObject.architecture.template_surface_slots_isolated COMMAND iiActorObjectTemplateSurfaceSlotPlanTests)
    add_executable(iiActorObjectTemplateSurfaceEditRemapTests tests/TemplateSurfaceEditRemapTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceEditRemap>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceEditRemapTests)
    add_test(NAME iiActorObject.architecture.template_surface_edit_remap_isolated COMMAND iiActorObjectTemplateSurfaceEditRemapTests)
    add_executable(iiActorObjectPointSelectionModuleTests tests/PointSelectionModuleTest.cpp tests/PointSelectionValidationHeadersTest.cpp
        $<TARGET_OBJECTS:iiActorObjectSelectionValidation> $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceSelectionRemap>
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceEditRemap> $<TARGET_OBJECTS:iiActorObjectMeshPointMaskValidation>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectPointSelectionModuleTests)
    add_test(NAME iiActorObject.architecture.point_selection_module_isolated COMMAND iiActorObjectPointSelectionModuleTests)
    add_executable(iiActorObjectProjectHistorySelectionTests tests/ProjectHistorySelectionTest.cpp
        $<TARGET_OBJECTS:iiActorObjectProjectHistorySelection> $<TARGET_OBJECTS:iiActorObjectSelectionValidation>
        $<TARGET_OBJECTS:iiActorObjectMeshPointMaskValidation> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectHistorySelectionTests)
    add_test(NAME iiActorObject.architecture.project_history_selection_isolated COMMAND iiActorObjectProjectHistorySelectionTests)
    add_executable(iiActorObjectTemplateSurfaceReferencesTests tests/TemplateSurfaceReferencesTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceReferences> $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceTopology>
        $<TARGET_OBJECTS:iiActorObjectSurfaceFitSettings> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceReferencesTests)
    add_test(NAME iiActorObject.architecture.template_surface_references_isolated COMMAND iiActorObjectTemplateSurfaceReferencesTests)
    add_executable(iiActorObjectTemplateSurfaceFollowersTests tests/TemplateSurfaceFollowersTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceFollowers> $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceReferences>
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceTopology> $<TARGET_OBJECTS:iiActorObjectSurfaceFitSettings>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceFollowersTests)
    add_test(NAME iiActorObject.architecture.template_surface_followers_isolated COMMAND iiActorObjectTemplateSurfaceFollowersTests)
    add_executable(iiActorObjectTemplateSurfaceResourcesTests tests/TemplateSurfaceResourcesTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceResources> $<TARGET_OBJECTS:iiActorObjectAssetData>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceResourcesTests)
    add_test(NAME iiActorObject.architecture.template_surface_resources_isolated COMMAND iiActorObjectTemplateSurfaceResourcesTests
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_executable(iiActorObjectTemplateSurfaceMaterialRoutingTests tests/TemplateSurfaceMaterialRoutingTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceMaterialRouting>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceMaterialRoutingTests)
    add_test(NAME iiActorObject.architecture.template_surface_material_routing_isolated
        COMMAND iiActorObjectTemplateSurfaceMaterialRoutingTests)
    add_executable(iiActorObjectTemplateSurfaceReplacementResourcesTests tests/TemplateSurfaceReplacementResourcesTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceReplacementResources> $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceResources>
        $<TARGET_OBJECTS:iiActorObjectAssetData> $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceReplacementResourcesTests)
    add_test(NAME iiActorObject.architecture.template_surface_replacement_resources_isolated
        COMMAND iiActorObjectTemplateSurfaceReplacementResourcesTests
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_executable(iiActorObjectTemplateSurfaceReplacementLayersTests tests/TemplateSurfaceReplacementLayersTest.cpp
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceReplacementLayers> $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceReplacementResources>
        $<TARGET_OBJECTS:iiActorObjectTemplateSurfaceResources> $<TARGET_OBJECTS:iiActorObjectAssetData>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectTemplateSurfaceReplacementLayersTests)
    add_test(NAME iiActorObject.architecture.template_surface_replacement_layers_isolated
        COMMAND iiActorObjectTemplateSurfaceReplacementLayersTests
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_executable(iiActorObjectGlbPointIdentityCodecTests tests/GlbPointIdentityCodecTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbPointIdentity>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbPointIdentityCodecTests)
    add_test(NAME iiActorObject.architecture.glb_point_identity_codec_isolated COMMAND iiActorObjectGlbPointIdentityCodecTests)
    add_executable(iiActorObjectGlbPointRestoreCoreTests tests/GlbPointRestoreCoreTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbPointRestoreCore>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbPointRestoreCoreTests)
    add_test(NAME iiActorObject.architecture.glb_point_restore_core_isolated COMMAND iiActorObjectGlbPointRestoreCoreTests)
    add_executable(iiActorObjectGlbFlatNormalsTests tests/GlbFlatNormalsTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbFlatNormals>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbFlatNormalsTests)
    add_test(NAME iiActorObject.architecture.glb_flat_normals_isolated COMMAND iiActorObjectGlbFlatNormalsTests)
    add_executable(iiActorObjectGlbBufferTests tests/GlbBufferTest.cpp $<TARGET_OBJECTS:iiActorObjectGlbBuffer>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbBufferTests)
    add_test(NAME iiActorObject.architecture.glb_buffer_isolated COMMAND iiActorObjectGlbBufferTests)
    add_executable(iiActorObjectGlbMaterialPlanTests tests/GlbMaterialPlanTest.cpp $<TARGET_OBJECTS:iiActorObjectGlbMaterialPlan>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMaterialPlanTests)
    add_test(NAME iiActorObject.architecture.glb_material_plan_isolated COMMAND iiActorObjectGlbMaterialPlanTests)
    add_executable(iiActorObjectGlbMaterialWriteTests tests/GlbMaterialWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbMaterialWrite> $<TARGET_OBJECTS:iiActorObjectGlbBuffer>
        $<TARGET_OBJECTS:iiActorObjectAssetData>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMaterialWriteTests)
    add_test(NAME iiActorObject.architecture.glb_material_write_isolated COMMAND iiActorObjectGlbMaterialWriteTests
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_executable(iiActorObjectGlbSceneWriteTests tests/GlbSceneWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbSceneWrite> $<TARGET_OBJECTS:iiActorObjectGlbBuffer>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbSceneWriteTests)
    add_test(NAME iiActorObject.architecture.glb_scene_write_isolated COMMAND iiActorObjectGlbSceneWriteTests)
    add_executable(iiActorObjectGlbTangentPlanTests tests/GlbTangentPlanTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbTangentPlan> $<TARGET_OBJECTS:iiActorObjectMeshTangents>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation> $<TARGET_OBJECTS:iiActorObjectFoundation>
        src/third_party/mikktspace/mikktspace.cpp)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTangentPlanTests)
    add_test(NAME iiActorObject.architecture.glb_tangent_plan_isolated COMMAND iiActorObjectGlbTangentPlanTests)
    add_executable(iiActorObjectGlbTangentWriteTests tests/GlbTangentWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbTangentWrite> $<TARGET_OBJECTS:iiActorObjectGlbPointAttributeWrite>
        $<TARGET_OBJECTS:iiActorObjectGlbBuffer> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbTangentWriteTests)
    add_test(NAME iiActorObject.architecture.glb_tangent_write_isolated COMMAND iiActorObjectGlbTangentWriteTests)
    add_executable(iiActorObjectGlbSharedMeshWriteTests tests/GlbSharedMeshWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbSharedMeshWrite> $<TARGET_OBJECTS:iiActorObjectGlbPointAttributeWrite>
        $<TARGET_OBJECTS:iiActorObjectGlbBuffer> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbSharedMeshWriteTests)
    add_test(NAME iiActorObject.architecture.glb_shared_mesh_write_isolated COMMAND iiActorObjectGlbSharedMeshWriteTests)
    add_executable(iiActorObjectGlbMeshExtrasWriteTests tests/GlbMeshExtrasWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbMeshExtrasWrite> $<TARGET_OBJECTS:iiActorObjectGlbPointIdentity>
        $<TARGET_OBJECTS:iiActorObjectGlbBuffer> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMeshExtrasWriteTests)
    add_test(NAME iiActorObject.architecture.glb_mesh_extras_write_isolated COMMAND iiActorObjectGlbMeshExtrasWriteTests)
    add_executable(iiActorObjectGlbPrimitivePlanTests tests/GlbPrimitivePlanTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbPrimitivePlan> $<TARGET_OBJECTS:iiActorObjectGlbTangentSnapshot>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation> $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbPrimitivePlanTests)
    add_test(NAME iiActorObject.architecture.glb_primitive_plan_isolated COMMAND iiActorObjectGlbPrimitivePlanTests)
    add_executable(iiActorObjectFbxTangentProjectionTests tests/FbxTangentProjectionTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxTangentProjection> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxTangentProjectionTests)
    add_test(NAME iiActorObject.architecture.fbx_tangent_projection_isolated COMMAND iiActorObjectFbxTangentProjectionTests)
    add_executable(iiActorObjectGlbFbxTangentRouteTests tests/GlbFbxTangentRouteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbFbxTangentRoute> $<TARGET_OBJECTS:iiActorObjectFbxTangentProjection>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation> $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbFbxTangentRouteTests)
    add_test(NAME iiActorObject.architecture.glb_fbx_tangent_route_isolated COMMAND iiActorObjectGlbFbxTangentRouteTests)
    add_executable(iiActorObjectGlbMeshWriteTests tests/GlbMeshWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbMeshWrite> $<TARGET_OBJECTS:iiActorObjectGlbPrimitivePlan>
        $<TARGET_OBJECTS:iiActorObjectGlbFbxTangentRoute> $<TARGET_OBJECTS:iiActorObjectFbxTangentProjection>
        $<TARGET_OBJECTS:iiActorObjectGlbMaterialPlan> $<TARGET_OBJECTS:iiActorObjectGlbTangentPlan>
        $<TARGET_OBJECTS:iiActorObjectGlbTangentWrite> $<TARGET_OBJECTS:iiActorObjectGlbSharedMeshWrite>
        $<TARGET_OBJECTS:iiActorObjectGlbMeshExtrasWrite> $<TARGET_OBJECTS:iiActorObjectGlbPointAttributeWrite>
        $<TARGET_OBJECTS:iiActorObjectGlbPointIdentity> $<TARGET_OBJECTS:iiActorObjectGlbBuffer>
        $<TARGET_OBJECTS:iiActorObjectMeshTangents> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>
        $<TARGET_OBJECTS:iiActorObjectFoundation> src/third_party/mikktspace/mikktspace.cpp)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMeshWriteTests)
    add_test(NAME iiActorObject.architecture.glb_mesh_write_isolated COMMAND iiActorObjectGlbMeshWriteTests)
    add_executable(iiActorObjectGlbSkinWriteTests tests/GlbSkinWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbSkinWrite> $<TARGET_OBJECTS:iiActorObjectGlbBuffer>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbSkinWriteTests)
    add_test(NAME iiActorObject.architecture.glb_skin_write_isolated COMMAND iiActorObjectGlbSkinWriteTests)
    add_executable(iiActorObjectGlbContainerWriteTests tests/GlbContainerWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbContainerWrite> $<TARGET_OBJECTS:iiActorObjectGlbBuffer>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbContainerWriteTests)
    add_test(NAME iiActorObject.architecture.glb_container_write_isolated COMMAND iiActorObjectGlbContainerWriteTests)
    add_executable(iiActorObjectGlbDocumentWriteTests tests/GlbDocumentWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbDocumentWrite> $<TARGET_OBJECTS:iiActorObjectGlbSceneWrite>
        $<TARGET_OBJECTS:iiActorObjectGlbMaterialWrite> $<TARGET_OBJECTS:iiActorObjectGlbBuffer>
        $<TARGET_OBJECTS:iiActorObjectAssetData>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbDocumentWriteTests)
    add_test(NAME iiActorObject.architecture.glb_document_write_isolated COMMAND iiActorObjectGlbDocumentWriteTests)
    add_executable(iiActorObjectFbxResourcesTests tests/FbxResourcesTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxResources>
        $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectAssetData>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxResourcesTests)
    add_test(NAME iiActorObject.architecture.fbx_resources_isolated COMMAND iiActorObjectFbxResourcesTests)
    add_executable(iiActorObjectFbxResourceWriteTests tests/FbxResourceWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxResourceWrite> $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes>
        $<TARGET_OBJECTS:iiActorObjectFbxBinary> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectAssetData>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxResourceWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_resource_write_isolated COMMAND iiActorObjectFbxResourceWriteTests
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_executable(iiActorObjectFbxResolverHeaderTests tests/FbxResolverHeaderTest.cpp)
    target_link_libraries(iiActorObjectFbxResolverHeaderTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectFbxResolverHeaderTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.fbx_resolver_header.standalone COMMAND iiActorObjectFbxResolverHeaderTests)
    add_executable(iiActorObjectFbxSkeletonTests tests/FbxSkeletonReadTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxSkeleton>
        $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSkeletonTests)
    add_test(NAME iiActorObject.architecture.fbx_skeleton_isolated COMMAND iiActorObjectFbxSkeletonTests)
    add_executable(iiActorObjectFbxSkeletonWriteTests tests/FbxSkeletonWriteTest.cpp src/Core/Transform.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxSkeletonWrite>
        $<TARGET_OBJECTS:iiActorObjectFbxSceneMetadataWrite> $<TARGET_OBJECTS:iiActorObjectFbxPoseWrite> $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes>
        $<TARGET_OBJECTS:iiActorObjectFbxBinary>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSkeletonWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_skeleton_write_isolated COMMAND iiActorObjectFbxSkeletonWriteTests)
    add_executable(iiActorObjectFbxExternalIdentityTests tests/FbxExternalIdentityReadTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxExternalIdentityRead> $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation> $<TARGET_OBJECTS:iiActorObjectGlbUfbxTestSupport>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxExternalIdentityTests)
    add_test(NAME iiActorObject.architecture.fbx_external_identity_isolated COMMAND iiActorObjectFbxExternalIdentityTests)
    add_executable(iiActorObjectFbxExternalIdentityWriteTests tests/FbxExternalIdentityWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxExternalIdentityWrite> $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes>
        $<TARGET_OBJECTS:iiActorObjectFbxBinary> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxExternalIdentityWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_external_identity_write_isolated COMMAND iiActorObjectFbxExternalIdentityWriteTests)
    add_executable(iiActorObjectFbxSceneMetadataTests tests/FbxSceneMetadataTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxSceneMetadata> $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectGlbUfbxTestSupport>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSceneMetadataTests)
    add_test(NAME iiActorObject.architecture.fbx_scene_metadata_isolated COMMAND iiActorObjectFbxSceneMetadataTests)
    add_executable(iiActorObjectFbxSceneMetadataWriteTests tests/FbxSceneMetadataWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxSceneMetadataWrite> $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes>
        $<TARGET_OBJECTS:iiActorObjectFbxBinary> $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSceneMetadataWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_scene_metadata_write_isolated COMMAND iiActorObjectFbxSceneMetadataWriteTests)
    add_executable(iiActorObjectFbxSkinTests tests/FbxSkinReadTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxSkin>
        $<TARGET_OBJECTS:iiActorObjectFbxMetadata>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSkinTests)
    add_test(NAME iiActorObject.architecture.fbx_skin_isolated COMMAND iiActorObjectFbxSkinTests)
    add_executable(iiActorObjectFbxSkinWriteTests tests/FbxSkinWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxSkinWrite> $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes>
        $<TARGET_OBJECTS:iiActorObjectFbxBinary> $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSkinWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_skin_write_isolated COMMAND iiActorObjectFbxSkinWriteTests)
    add_executable(iiActorObjectFbxMorphShapeTests tests/FbxMorphShapeTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxMorphShape>
        $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMorphShapeTests)
    add_test(NAME iiActorObject.architecture.fbx_morph_shape_isolated COMMAND iiActorObjectFbxMorphShapeTests)
    add_executable(iiActorObjectFbxTangentReadTests tests/FbxTangentReadTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxTangentRead>
        $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxTangentReadTests)
    add_test(NAME iiActorObject.architecture.fbx_tangent_read_isolated COMMAND iiActorObjectFbxTangentReadTests)
    add_executable(iiActorObjectFbxMorphChannelReadTests tests/FbxMorphChannelReadTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxMorphChannelRead>
        $<TARGET_OBJECTS:iiActorObjectFbxProgressiveMetadata> $<TARGET_OBJECTS:iiActorObjectFbxMorphShape>
        $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMorphChannelReadTests)
    add_test(NAME iiActorObject.architecture.fbx_morph_channel_read_isolated COMMAND iiActorObjectFbxMorphChannelReadTests)
    add_executable(iiActorObjectFbxGeometryReadTests tests/FbxGeometryReadTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxGeometryRead>
        $<TARGET_OBJECTS:iiActorObjectFbxSkin> $<TARGET_OBJECTS:iiActorObjectFbxTopology>
        $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectFbxBinary>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxGeometryReadTests)
    add_test(NAME iiActorObject.architecture.fbx_geometry_read_isolated COMMAND iiActorObjectFbxGeometryReadTests)
    add_executable(iiActorObjectFbxMeshSceneReadTests tests/FbxMeshSceneReadTest.cpp
        src/third_party/ufbx/ufbx.cpp $<TARGET_OBJECTS:iiActorObjectFbxMeshSceneRead>
        $<TARGET_OBJECTS:iiActorObjectFbxGeometryRead> $<TARGET_OBJECTS:iiActorObjectFbxMorphChannelRead>
        $<TARGET_OBJECTS:iiActorObjectFbxExternalIdentityRead> $<TARGET_OBJECTS:iiActorObjectFbxTangentRead>
        $<TARGET_OBJECTS:iiActorObjectFbxProgressiveMetadata> $<TARGET_OBJECTS:iiActorObjectFbxMorphShape>
        $<TARGET_OBJECTS:iiActorObjectFbxSkin> $<TARGET_OBJECTS:iiActorObjectFbxTopology>
        $<TARGET_OBJECTS:iiActorObjectFbxMetadata> $<TARGET_OBJECTS:iiActorObjectFbxBinary>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMeshSceneReadTests)
    add_test(NAME iiActorObject.architecture.fbx_mesh_scene_read_isolated COMMAND iiActorObjectFbxMeshSceneReadTests)
    add_executable(iiActorObjectFbxAnimationStageTests tests/FbxAnimationStageTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxAnimationStage> $<TARGET_OBJECTS:iiActorObjectSkeletalMotion>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxAnimationStageTests)
    add_test(NAME iiActorObject.architecture.fbx_animation_stage_isolated COMMAND iiActorObjectFbxAnimationStageTests)
    add_executable(iiActorObjectProjectCodecTests tests/ProjectCodecTest.cpp
        $<TARGET_OBJECTS:iiActorObjectProjectCodec> $<TARGET_OBJECTS:iiActorObjectProjectVersion>
        $<TARGET_OBJECTS:iiActorObjectSurfaceFitSettings> $<TARGET_OBJECTS:iiActorObjectAssetData>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectSkeletalMotion>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectCodecTests)
    add_test(NAME iiActorObject.architecture.project_codec COMMAND iiActorObjectProjectCodecTests)
    add_executable(iiActorObjectProjectRigArchiveTests tests/ProjectRigArchiveTest.cpp tests/ProjectRigRestoreHeaderTest.cpp
        $<TARGET_OBJECTS:iiActorObjectProjectRigArchive>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectRigArchiveTests)
    add_test(NAME iiActorObject.architecture.project_rig_archive_isolated COMMAND iiActorObjectProjectRigArchiveTests)
    add_executable(iiActorObjectProjectAdjustmentValidationTests tests/ProjectAdjustmentValidationTest.cpp
        $<TARGET_OBJECTS:iiActorObjectProjectAdjustmentValidation>
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectAdjustmentValidationTests)
    add_test(NAME iiActorObject.architecture.project_adjustment_validation_isolated COMMAND iiActorObjectProjectAdjustmentValidationTests)
    add_executable(iiActorObjectSurfaceFitSettingsTests tests/SurfaceFitSettingsTest.cpp
        $<TARGET_OBJECTS:iiActorObjectSurfaceFitSettings>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectSurfaceFitSettingsTests)
    add_test(NAME iiActorObject.architecture.surface_fit_settings COMMAND iiActorObjectSurfaceFitSettingsTests)
    add_executable(iiActorObjectProjectTemplateAdapterTests tests/ProjectTemplateAdapterTest.cpp
        $<TARGET_OBJECTS:iiActorObjectProjectTemplateAdapter> $<TARGET_OBJECTS:iiActorObjectSurfaceFitSettings>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectTemplateAdapterTests)
    add_test(NAME iiActorObject.architecture.project_template_adapter COMMAND iiActorObjectProjectTemplateAdapterTests)
    add_executable(iiActorObjectProjectDataHeadersTests tests/ProjectDataHeadersTest.cpp
        tests/ProjectDataMeshPointMaskHeaderTest.cpp tests/ProjectDataMeshPointMaskValidationHeaderTest.cpp tests/ProjectDataObject3DEditLayerDataHeaderTest.cpp tests/ProjectDataObject3DPoseProgramDataHeaderTest.cpp tests/ProjectDataObject3DProjectOptionsHeaderTest.cpp tests/ProjectDataObject3DTemplateMorphDataHeaderTest.cpp tests/ProjectDataObject3DTemplateTypesHeaderTest.cpp tests/ProjectDataSurfaceFitSettingsHeaderTest.cpp tests/ExternalIdentityHeaderTest.cpp
        $<TARGET_OBJECTS:iiActorObjectSurfaceFitSettings>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectProjectDataHeadersTests)
    add_test(NAME iiActorObject.architecture.project_data_headers COMMAND iiActorObjectProjectDataHeadersTests)
    add_executable(iiActorObjectGlbAnimationWriteTests tests/GlbAnimationWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbAnimationWrite> $<TARGET_OBJECTS:iiActorObjectGlbBuffer>
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectSkeletalMotion>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbAnimationWriteTests)
    add_test(NAME iiActorObject.architecture.glb_animation_write_isolated COMMAND iiActorObjectGlbAnimationWriteTests)
    add_executable(iiActorObjectGlbAnimationOptionsHeaderTests tests/GlbAnimationOptionsHeaderTest.cpp)
    target_link_libraries(iiActorObjectGlbAnimationOptionsHeaderTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectGlbAnimationOptionsHeaderTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.glb_animation_options.standalone COMMAND iiActorObjectGlbAnimationOptionsHeaderTests)
    add_executable(iiActorObjectFbxMorphWriteTests tests/FbxMorphWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxMorphWrite> $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes>
        $<TARGET_OBJECTS:iiActorObjectFbxBinary> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMorphWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_morph_write_isolated COMMAND iiActorObjectFbxMorphWriteTests)
    add_executable(iiActorObjectFbxMorphChannelsWriteTests tests/FbxMorphChannelsWriteTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxMorphChannelsWrite> $<TARGET_OBJECTS:iiActorObjectMorphChannels>
        $<TARGET_OBJECTS:iiActorObjectFbxMorphWrite> $<TARGET_OBJECTS:iiActorObjectFbxBinary>
        $<TARGET_OBJECTS:iiActorObjectFbxSceneNodes> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxMorphChannelsWriteTests)
    add_test(NAME iiActorObject.architecture.fbx_morph_channels_write_isolated COMMAND iiActorObjectFbxMorphChannelsWriteTests)
    add_executable(iiActorObjectFbxSharedGeometryTests tests/FbxSharedGeometryTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxSharedGeometry> $<TARGET_OBJECTS:iiActorObjectFbxBinary> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxSharedGeometryTests)
    add_test(NAME iiActorObject.architecture.fbx_shared_geometry_isolated COMMAND iiActorObjectFbxSharedGeometryTests)
    add_executable(iiActorObjectFbxGeometryRegistryTests tests/FbxGeometryRegistryTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFbxGeometryRegistry> $<TARGET_OBJECTS:iiActorObjectFbxSharedGeometry>
        $<TARGET_OBJECTS:iiActorObjectFbxBinary> $<TARGET_OBJECTS:iiActorObjectFoundation>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFbxGeometryRegistryTests)
    add_test(NAME iiActorObject.architecture.fbx_geometry_registry_isolated COMMAND iiActorObjectFbxGeometryRegistryTests)
    # This test links only the foundation object and the C++ runtime, proving
    # that the low-level transform module is usable without the engine library.
    add_executable(iiActorObjectFoundationTests tests/FoundationModuleTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectFoundationTests)
    add_test(NAME iiActorObject.architecture.foundation_isolated COMMAND iiActorObjectFoundationTests)
    add_executable(iiActorObjectSkeletalMotionTests tests/SkeletalMotionModuleTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectSkeletalMotion>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectSkeletalMotionTests)
    add_test(NAME iiActorObject.architecture.skeletal_motion_isolated COMMAND iiActorObjectSkeletalMotionTests)
    add_executable(iiActorObjectMeshFoundationTests tests/MeshFoundationModuleTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectMeshFoundation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectMeshFoundationTests)
    add_test(NAME iiActorObject.architecture.mesh_foundation_isolated COMMAND iiActorObjectMeshFoundationTests)
    add_executable(iiActorObjectDataModuleTests tests/Object3DDataModuleTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectSkeletalMotion>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation> $<TARGET_OBJECTS:iiActorObjectAssetData>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectDataModuleTests)
    add_test(NAME iiActorObject.architecture.object_data_isolated COMMAND iiActorObjectDataModuleTests)
    # Exercise the GLB attribute reader without linking the actor engine.
    add_executable(iiActorObjectGlbMeshAttributesTests tests/GlbMeshAttributesTest.cpp
        $<TARGET_OBJECTS:iiActorObjectGlbMeshAttributes> $<TARGET_OBJECTS:iiActorObjectGlbJson>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectGlbMeshAttributesTests)
    target_link_libraries(iiActorObjectGlbMeshAttributesTests PRIVATE iiJson::iiJson)
    add_test(NAME iiActorObject.architecture.glb_mesh_attributes_isolated COMMAND iiActorObjectGlbMeshAttributesTests)
    add_executable(iiActorObjectDeformationTests tests/SkeletalDeformationModuleTest.cpp
        $<TARGET_OBJECTS:iiActorObjectFoundation> $<TARGET_OBJECTS:iiActorObjectSkeletalMotion>
        $<TARGET_OBJECTS:iiActorObjectMeshFoundation> $<TARGET_OBJECTS:iiActorObjectDeformation>)
    iiActorObjectConfigureEmbeddedTarget(iiActorObjectDeformationTests)
    add_test(NAME iiActorObject.architecture.deformation_isolated COMMAND iiActorObjectDeformationTests)
    add_executable(iiActorObjectRigSampleCatalogHeadersTests
        tests/RigSampleCatalogCoreHeaderTest.cpp tests/RigSampleCatalogExportHeaderTest.cpp
        tests/RigSampleCatalogCompatibilityHeaderTest.cpp)
    target_link_libraries(iiActorObjectRigSampleCatalogHeadersTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectRigSampleCatalogHeadersTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.rig_sample_catalog_headers.standalone COMMAND iiActorObjectRigSampleCatalogHeadersTests)
    add_executable(iiActorObjectProgressiveHeadersTests
        tests/ProgressiveMorphDataHeaderTest.cpp tests/ProgressiveMorphCoreHeaderTest.cpp tests/ProgressiveMorphInterchangeHeaderTest.cpp
        tests/ProgressiveMorphCompatibilityHeaderTest.cpp)
    target_link_libraries(iiActorObjectProgressiveHeadersTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectProgressiveHeadersTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.progressive_headers.standalone COMMAND iiActorObjectProgressiveHeadersTests)
    add_executable(iiActorObjectMeshHeadersTests
        tests/BoneIndexHeaderTest.cpp tests/MeshDataHeaderTest.cpp tests/MeshTopologyCoreHeaderTest.cpp
        tests/MeshGeometryCoreHeaderTest.cpp tests/MeshCompatibilityHeaderTest.cpp
        tests/SurfaceBindingCoreHeaderTest.cpp tests/SurfaceAttachmentHeaderTest.cpp
        tests/SurfaceCompatibilityHeaderTest.cpp tests/Object3DDataHeaderTest.cpp tests/Object3DMaterialHeaderTest.cpp tests/Object3DMeshHeaderTest.cpp
        tests/Object3DAssetCompatibilityHeaderTest.cpp tests/MeshSelectionCoreHeaderTest.cpp
        tests/MeshSelectionAssetHeaderTest.cpp tests/MeshSelectionCompatibilityHeaderTest.cpp
        tests/SkeletalMeshCoreHeaderTest.cpp tests/SkeletalMeshComponentHeaderTest.cpp)
    target_link_libraries(iiActorObjectMeshHeadersTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectMeshHeadersTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.mesh_headers.standalone COMMAND iiActorObjectMeshHeadersTests)
    add_executable(iiActorObjectProgressiveMorphRigTests tests/ProgressiveMorphRigTest.cpp src/third_party/ufbx/ufbx.cpp)
    target_link_libraries(iiActorObjectProgressiveMorphRigTests PRIVATE iiActorObject::iiActorObject)
    target_compile_features(iiActorObjectProgressiveMorphRigTests PRIVATE cxx_std_23)
    set_target_properties(iiActorObjectProgressiveMorphRigTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case curve invalid project lifetime split sparse sparse_limits)
        add_test(NAME iiActorObject.progressive_morph.${_case} COMMAND iiActorObjectProgressiveMorphRigTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.progressive_morph.reference COMMAND iiActorObjectProgressiveMorphRigTests reference
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/progressive-morph-ascii.fbx")
    add_test(NAME iiActorObject.progressive_morph.import COMMAND iiActorObjectProgressiveMorphRigTests import
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/progressive-morph-ascii.fbx")
    add_test(NAME iiActorObject.progressive_morph.export COMMAND iiActorObjectProgressiveMorphRigTests export
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/progressive-morph-ascii.fbx")
    add_test(NAME iiActorObject.progressive_morph.export_reference COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProgressiveMorphFbx.py" "$<TARGET_FILE:iiActorObjectProgressiveMorphRigTests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/progressive-morph-ascii.fbx")
    add_test(NAME iiActorObject.progressive_morph.split_reference COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProgressiveSplitFbx.py" "$<TARGET_FILE:iiActorObjectProgressiveMorphRigTests>")
    add_test(NAME iiActorObject.progressive_morph.sparse_reference COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProgressiveSparseFbx.py" "$<TARGET_FILE:iiActorObjectProgressiveMorphRigTests>")
    add_test(NAME iiActorObject.progressive_morph.archive_reference COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProgressiveMorphProject.py" "$<TARGET_FILE:iiActorObjectProgressiveMorphRigTests>")
    add_executable(iiActorObjectProgressiveMorphAnimationTests tests/ProgressiveMorphAnimationTest.cpp src/third_party/ufbx/ufbx.cpp)
    target_link_libraries(iiActorObjectProgressiveMorphAnimationTests PRIVATE iiActorObject::iiActorObject)
    target_compile_features(iiActorObjectProgressiveMorphAnimationTests PRIVATE cxx_std_23)
    set_target_properties(iiActorObjectProgressiveMorphAnimationTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case import project validation shared cubic takes export sensitivity order export_takes)
        add_test(NAME iiActorObject.progressive_animation.${_case} COMMAND iiActorObjectProgressiveMorphAnimationTests ${_case}
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/progressive-animated-ascii.fbx")
    endforeach()
    add_test(NAME iiActorObject.progressive_animation.export_reference COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProgressiveAnimationFbx.py" "$<TARGET_FILE:iiActorObjectProgressiveMorphAnimationTests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/progressive-animated-ascii.fbx")
    add_test(NAME iiActorObject.progressive_animation.takes_reference COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProgressiveAnimationTakes.py" "$<TARGET_FILE:iiActorObjectProgressiveMorphAnimationTests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/progressive-animated-ascii.fbx")
    add_test(NAME iiActorObject.fbx_animation_import.logical COMMAND iiActorObjectFbxAnimationImportTests logical)
    foreach(_case roundtrip external nonlinear validation)
        add_test(NAME iiActorObject.fbx_animation_import.${_case} COMMAND iiActorObjectFbxAnimationImportTests ${_case}
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/animated-parent-ascii.fbx")
    endforeach()
    add_executable(iiActorObjectFbxAnimationImportReferenceTests tests/FbxAnimationImportReferenceTest.cpp src/third_party/ufbx/ufbx.cpp)
    target_compile_features(iiActorObjectFbxAnimationImportReferenceTests PRIVATE cxx_std_23)
    set_target_properties(iiActorObjectFbxAnimationImportReferenceTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.fbx_animation_import.independent_source COMMAND iiActorObjectFbxAnimationImportReferenceTests
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/animated-parent-ascii.fbx")
    add_test(NAME iiActorObject.fbx_animation_import.independent_cubic COMMAND iiActorObjectFbxAnimationImportReferenceTests
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/animated-parent-ascii.fbx" cubic)
    add_test(NAME iiActorObject.fbx_animation_import.independent_pivot COMMAND iiActorObjectFbxAnimationImportReferenceTests
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/animated-parent-ascii.fbx" pivot)
    add_executable(iiActorObjectPoseFrameTests tests/PoseFramesTest.cpp)
    target_link_libraries(iiActorObjectPoseFrameTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectPoseFrameTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case basic scaled reflection local_reflection both_reflections root large_origin dynamic_bind failures)
        add_test(NAME iiActorObject.pose_frame.${_case} COMMAND iiActorObjectPoseFrameTests ${_case})
    endforeach()
    add_executable(iiActorObjectPoseSnapshotReference tests/DumpPoseSnapshotReference.cpp)
    add_executable(iiActorObjectReferencePoseTests tests/Object3DReferencePoseTest.cpp)
    target_link_libraries(iiActorObjectReferencePoseTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectReferencePoseTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case query direction_queries ordered ordered_invalid ordered_project ordered_layered ordered_malformed runtime history layered inherited cleared_inherited morphs morphs_render morphs_dq atomic project exchange rig lifetime malformed group_project group_malformed)
        add_test(NAME iiActorObject.reference_pose.${_case} COMMAND iiActorObjectReferencePoseTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.reference_pose.fixture COMMAND iiActorObjectReferencePoseTests fixture "${CMAKE_CURRENT_BINARY_DIR}/reference-pose/Fixture-v13.ii3dproject")
    set_tests_properties(iiActorObject.reference_pose.fixture PROPERTIES FIXTURES_SETUP iiActorObject_reference_pose)
    add_test(NAME iiActorObject.reference_pose.ordered_fixture COMMAND iiActorObjectReferencePoseTests ordered_fixture "${CMAKE_CURRENT_BINARY_DIR}/reference-pose/Fixture-v15.ii3dproject")
    set_tests_properties(iiActorObject.reference_pose.ordered_fixture PROPERTIES FIXTURES_SETUP iiActorObject_ordered_reference_pose)
    add_test(NAME iiActorObject.reference_pose.group_format_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyGroupedPoseProject.py" "$<TARGET_FILE:iiActorObjectReferencePoseTests>")
    target_link_libraries(iiActorObjectPoseSnapshotReference PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectPoseSnapshotReference PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(iiActorObjectMeshSurfaceProjectionTests tests/MeshSurfaceProjectionTest.cpp)
    target_link_libraries(iiActorObjectMeshSurfaceProjectionTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectMeshSurfaceProjectionTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.surface_collision.projection COMMAND iiActorObjectMeshSurfaceProjectionTests)
    add_executable(iiActorObjectSurfaceFairingTests tests/SurfaceFairingTest.cpp)
    target_link_libraries(iiActorObjectSurfaceFairingTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case energy pins strain budgets noop validation settings project legacy_v10)
        add_test(NAME iiActorObject.surface_fairing.${_case} COMMAND iiActorObjectSurfaceFairingTests ${_case} "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v10.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing/Fixture-v12.ii3dproject")
    endforeach()
    add_test(NAME iiActorObject.surface_fairing.legacy_v11 COMMAND iiActorObjectSurfaceFairingTests legacy_v11 "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v11.ii3dproject")
    set_tests_properties(iiActorObject.surface_fairing.project PROPERTIES FIXTURES_SETUP iiActorObject_fairing_project)
    add_executable(iiActorObjectAutomaticFittingTests tests/AutomaticFittingTest.cpp)
    target_link_libraries(iiActorObjectAutomaticFittingTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case settings generation followers basis_followers graph edits history masks serialization atomic budgets evaluated_sculpt evaluated_external evaluated_order evaluated_followers evaluated_external_followers evaluated_atomic)
        add_test(NAME iiActorObject.automatic_fitting.${_case} COMMAND iiActorObjectAutomaticFittingTests ${_case}
            "${CMAKE_CURRENT_BINARY_DIR}/automatic-fitting/Fixture-v10.ii3dproject")
    endforeach()
    add_test(NAME iiActorObject.automatic_fitting.legacy_v9 COMMAND iiActorObjectAutomaticFittingTests legacy_v9 "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v9.ii3dproject")
    add_test(NAME iiActorObject.automatic_fitting.legacy_v9_evaluated COMMAND iiActorObjectAutomaticFittingTests legacy_v9 "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v9-evaluated-surface.ii3dproject")
    set_tests_properties(iiActorObject.automatic_fitting.serialization PROPERTIES FIXTURES_SETUP iiActorObject_automatic_fitting_project)
    add_executable(iiActorObjectSurfaceCollisionTests tests/SurfaceCollisionTest.cpp)
    target_link_libraries(iiActorObjectSurfaceCollisionTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case triangle_pairs cube nested concave invalid fitting masks face_constraints history budgets boundary_gradient escape)
        add_test(NAME iiActorObject.surface_collision.${_case} COMMAND iiActorObjectSurfaceCollisionTests ${_case})
    endforeach()
    add_executable(iiActorObjectMeshSymmetryTests tests/MeshSymmetryTest.cpp)
    target_link_libraries(iiActorObjectMeshSymmetryTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case mapping positions morphs normal_seams failures explicit_pairs masks layers spatial)
        add_test(NAME iiActorObject.mesh_symmetry.${_case} COMMAND iiActorObjectMeshSymmetryTests ${_case})
    endforeach()
    add_executable(iiActorObjectPoseRigTests tests/PoseRigTest.cpp)
    target_link_libraries(iiActorObjectPoseRigTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case bvh evaluate clone overlay codec bake bake_dual_quaternion project)
        add_test(NAME iiActorObject.pose_rig.${_case} COMMAND iiActorObjectPoseRigTests ${_case})
    endforeach()
    add_executable(iiActorObjectTemplateGroundingTests tests/TemplateGroundingTest.cpp)
    target_link_libraries(iiActorObjectTemplateGroundingTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case translation generation skin validation codec)
        add_test(NAME iiActorObject.template_grounding.${_case} COMMAND iiActorObjectTemplateGroundingTests ${_case})
    endforeach()
    add_executable(iiActorObjectControlPointBindingsTests tests/ControlPointBindingsTest.cpp)
    target_link_libraries(iiActorObjectControlPointBindingsTests PRIVATE iiActorObject::iiActorObject)
    add_executable(iiActorObjectTemplateSurfaceSwapTests tests/TemplateSurfaceSwapTest.cpp)
    target_link_libraries(iiActorObjectTemplateSurfaceSwapTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectTemplateSurfaceSwapTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case swap safeguards sharing grounding visibility_triangles removal addition addition_graph composition)
        add_test(NAME iiActorObject.template_surface_swap.${_case} COMMAND iiActorObjectTemplateSurfaceSwapTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.template_surface_swap.replacement_resources
        COMMAND iiActorObjectTemplateSurfaceSwapTests replacement_resources "-"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_executable(iiActorObjectPortableNumberTests tests/PortableNumberParsingTest.cpp)
    target_compile_features(iiActorObjectPortableNumberTests PRIVATE cxx_std_23)
    target_include_directories(iiActorObjectPortableNumberTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/include")
    set_target_properties(iiActorObjectPortableNumberTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.portable_numbers COMMAND iiActorObjectPortableNumberTests)
    foreach(_case weighted scale basis graph codec parser numbers covered skin)
        add_test(NAME iiActorObject.control_point_bindings.${_case} COMMAND iiActorObjectControlPointBindingsTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.control_point_bindings.basis_project_fixture COMMAND iiActorObjectControlPointBindingsTests
        basis_project_fixture "${CMAKE_CURRENT_BINARY_DIR}/control-point-basis/fixture.ii3dproject")
    set_tests_properties(iiActorObject.control_point_bindings.basis_project_fixture PROPERTIES FIXTURES_SETUP iiActorObject_control_basis)
    add_test(NAME iiActorObject.control_point_bindings.basis_project_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifySurfaceBasisProject.py" "$<TARGET_FILE:iiActorObjectControlPointBindingsTests>"
        "${CMAKE_CURRENT_BINARY_DIR}/control-point-basis/fixture.ii3dproject")
    set_tests_properties(iiActorObject.control_point_bindings.basis_project_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_control_basis)
    add_executable(iiActorObjectImagesTests tests/Object3DImagesTest.cpp)
    target_link_libraries(iiActorObjectImagesTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case immutable validation formats history)
        add_test(NAME iiActorObject.images.${_case} COMMAND iiActorObjectImagesTests ${_case} "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png" "${CMAKE_CURRENT_BINARY_DIR}/images")
    endforeach()
    set_tests_properties(iiActorObject.images.formats PROPERTIES FIXTURES_SETUP iiActorObject_images)
    add_executable(iiActorObjectMeshTangentTests tests/MeshTangentTest.cpp)
    target_link_libraries(iiActorObjectMeshTangentTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectMeshTangentTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case seams deformation validation)
        add_test(NAME iiActorObject.mesh_tangents.${_case} COMMAND iiActorObjectMeshTangentTests ${_case})
    endforeach()
    add_executable(iiActorObjectTemplateControlsTests tests/TemplateControlsTest.cpp)
    target_link_libraries(iiActorObjectTemplateControlsTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case signed products composition validation history codec)
        add_test(NAME iiActorObject.template_controls.${_case} COMMAND iiActorObjectTemplateControlsTests ${_case})
    endforeach()
    add_executable(iiActorObjectExternalEditsTests tests/Object3DExternalEditsTest.cpp)
    target_link_libraries(iiActorObjectExternalEditsTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectExternalEditsTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case changes noop identity conflicts attributes layers surfaces material_surface fbx_identity glb_triangles glb_auto grouped_morphs grouped_morphs_mixed upsert morph_attributes base_normals base_normals_advanced reordered malformed_protocol surface_chain)
        add_test(NAME iiActorObject.external_edits.${_case} COMMAND iiActorObjectExternalEditsTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.external_edits.glb_auto_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyGlbExternalIdentity.py" "$<TARGET_FILE:iiActorObjectExternalEditsTests>")
    add_test(NAME iiActorObject.external_edits.images COMMAND iiActorObjectExternalEditsTests images "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png" "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png")
    add_test(NAME iiActorObject.external_edits.grouped_fbx_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyGroupedMorphFbx.py" "$<TARGET_FILE:iiActorObjectExternalEditsTests>")
    add_test(NAME iiActorObject.external_edits.base_normal_fbx_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyBaseNormalFbx.py" "$<TARGET_FILE:iiActorObjectExternalEditsTests>")
    add_test(NAME iiActorObject.external_edits.base_normal_project_fixture COMMAND iiActorObjectExternalEditsTests
        base_normals_project_fixture "${CMAKE_CURRENT_BINARY_DIR}/base-normal/Fixture-v19.ii3dproject")
    set_tests_properties(iiActorObject.external_edits.base_normal_project_fixture PROPERTIES FIXTURES_SETUP iiActorObject_base_normal_project)
    add_executable(iiActorObjectFbxImportPrecisionTests tests/FbxImportPrecisionTest.cpp)
    add_executable(iiActorObjectFbxExternalImageTests tests/FbxExternalImageTest.cpp)
    target_link_libraries(iiActorObjectFbxExternalImageTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectFbxExternalImageTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case resolution limits missing embedded)
        add_test(NAME iiActorObject.fbx_external_image.${_case} COMMAND iiActorObjectFbxExternalImageTests ${_case}
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/external-textures-ascii.fbx"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/fbx/normal-map-ascii.fbx")
    endforeach()
    target_link_libraries(iiActorObjectFbxImportPrecisionTests PRIVATE iiActorObject::iiActorObject)
    add_test(NAME iiActorObject.fbx_import.precision COMMAND iiActorObjectFbxImportPrecisionTests)
    add_executable(iiActorObjectSkeletonScaleTests tests/SkeletonScaleTest.cpp)
    target_link_libraries(iiActorObjectSkeletonScaleTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case poses edits codec failures)
        add_test(NAME iiActorObject.skeleton_scale.${_case} COMMAND iiActorObjectSkeletonScaleTests ${_case})
    endforeach()
    add_executable(iiActorObjectSkeletonEditingTests tests/SkeletonEditingTest.cpp)
    target_link_libraries(iiActorObjectSkeletonEditingTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case order transform object_space reparent orientation failures rename_batch rename_atomic rename_layers layers layer_cycles nonuniform anchors formats)
        add_test(NAME iiActorObject.skeleton_editing.${_case} COMMAND iiActorObjectSkeletonEditingTests ${_case} "${CMAKE_CURRENT_BINARY_DIR}/skeleton-editing")
    endforeach()
    set_tests_properties(iiActorObject.skeleton_editing.formats PROPERTIES FIXTURES_SETUP iiActorObject_skeleton_editing)
    add_executable(iiActorObjectPaintingTests tests/MeshPaintingTest.cpp)
    target_link_libraries(iiActorObjectPaintingTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case stroke locked mask selection sculpt modes distance failures morphs symmetry clothing artist_normals)
        add_test(NAME iiActorObject.painting.${_case} COMMAND iiActorObjectPaintingTests ${_case})
    endforeach()
    add_executable(iiActorObjectNamedPointSelectionTests tests/NamedPointSelectionTest.cpp)
    target_link_libraries(iiActorObjectNamedPointSelectionTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectNamedPointSelectionTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case snapshot validation layered transition compatibility corruption)
        add_test(NAME iiActorObject.named_selection.${_case} COMMAND iiActorObjectNamedPointSelectionTests ${_case})
    endforeach()
    add_executable(iiActorObjectMeshMaskProjectionTests tests/MeshMaskProjectionTest.cpp)
    target_link_libraries(iiActorObjectMeshMaskProjectionTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectMeshMaskProjectionTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case projection render editor multiple batch validation)
        add_test(NAME iiActorObject.mesh_mask_projection.${_case} COMMAND iiActorObjectMeshMaskProjectionTests ${_case})
    endforeach()
    add_executable(iiActorObjectSelectionTransferTests tests/PointSelectionTransferTest.cpp)
    target_link_libraries(iiActorObjectSelectionTransferTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectSelectionTransferTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case transfer validation layered serialization history surface_v29)
        add_test(NAME iiActorObject.selection_transfer.${_case} COMMAND iiActorObjectSelectionTransferTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.selection_transfer.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifySelectionTransfer.py" "$<TARGET_FILE:iiActorObjectSelectionTransferTests>")
    if(IIACTOROBJECT_WITH_NSCHARACTER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman/base.obj")
        add_test(NAME iiActorObject.selection_transfer.human COMMAND iiActorObjectSelectionTransferTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
    endif()
    add_executable(iiActorObjectScreenSelectionTests tests/MeshScreenSelectionTest.cpp)
    target_link_libraries(iiActorObjectScreenSelectionTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectScreenSelectionTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case shapes perspective depth overrides deformed editor validation)
        add_test(NAME iiActorObject.screen_selection.${_case} COMMAND iiActorObjectScreenSelectionTests ${_case})
    endforeach()
    if(IIACTOROBJECT_WITH_NSCHARACTER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman/base.obj")
        add_test(NAME iiActorObject.screen_selection.human COMMAND iiActorObjectScreenSelectionTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
    endif()
    if(IIACTOROBJECT_WITH_NSCHARACTER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
        add_test(NAME iiActorObject.mesh_mask_projection.human COMMAND iiActorObjectMeshMaskProjectionTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
    endif()
    add_executable(iiActorObjectSkinSurfaceTests tests/SkinSurfaceTest.cpp)
    target_link_libraries(iiActorObjectSkinSurfaceTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case canonical projection transfer mirror conform layers attach)
        add_test(NAME iiActorObject.skin_surface.${_case} COMMAND iiActorObjectSkinSurfaceTests ${_case})
    endforeach()
    add_executable(iiActorObjectSkinNormalTieTests tests/SkinNormalTieTest.cpp)
    target_link_libraries(iiActorObjectSkinNormalTieTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectSkinNormalTieTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case opposite render ambiguous limits editor layered editor_atomic)
        add_test(NAME iiActorObject.skin_normal_ties.${_case} COMMAND iiActorObjectSkinNormalTieTests ${_case})
    endforeach()
    if(IIACTOROBJECT_WITH_NSCHARACTER)
        foreach(_case character character_lifetime)
            add_test(NAME iiActorObject.skin_normal_ties.${_case} COMMAND iiActorObjectSkinNormalTieTests ${_case})
        endforeach()
    endif()
    add_executable(iiActorObjectSkinNormalTieInterchangeTests tests/SkinNormalTieInterchangeTest.cpp)
    target_link_libraries(iiActorObjectSkinNormalTieInterchangeTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectSkinNormalTieInterchangeTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.skin_normal_ties.interchange_fixture COMMAND iiActorObjectSkinNormalTieInterchangeTests
        "${CMAKE_CURRENT_BINARY_DIR}/skin-normal-tie-interchange")
    set_tests_properties(iiActorObject.skin_normal_ties.interchange_fixture PROPERTIES FIXTURES_SETUP iiActorObject_skin_normal_tie_interchange)
    add_test(NAME iiActorObject.skin_normal_ties.interchange_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifySkinNormalTieFormats.py" "${CMAKE_CURRENT_BINARY_DIR}/skin-normal-tie-interchange")
    set_tests_properties(iiActorObject.skin_normal_ties.interchange_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_skin_normal_tie_interchange)
    add_executable(iiActorObjectLegacySurfaceAbiTests tests/LegacySurfaceAbiTest.cpp)
    target_link_libraries(iiActorObjectLegacySurfaceAbiTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectLegacySurfaceAbiTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.skin_surface.legacy_abi COMMAND iiActorObjectLegacySurfaceAbiTests)
    add_executable(iiActorObjectSkinGeodesicTests tests/SkinGeodesicTest.cpp)
    target_link_libraries(iiActorObjectSkinGeodesicTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectSkinGeodesicTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case surface manual seams unreachable validation editor)
        add_test(NAME iiActorObject.skin_geodesic.${_case} COMMAND iiActorObjectSkinGeodesicTests ${_case})
    endforeach()
    add_executable(iiActorObjectSkeletonEnvelopeTests tests/SkeletonEnvelopeTest.cpp)
    target_link_libraries(iiActorObjectSkeletonEnvelopeTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectSkeletonEnvelopeTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case inference branches editor validation)
        add_test(NAME iiActorObject.skeleton_envelopes.${_case} COMMAND iiActorObjectSkeletonEnvelopeTests ${_case})
    endforeach()
    add_executable(iiActorObjectTemplateRigTransferTests tests/TemplateRigTransferTest.cpp)
    target_link_libraries(iiActorObjectTemplateRigTransferTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectTemplateRigTransferTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case identity surface validation ownership automatic automatic_validation)
        add_test(NAME iiActorObject.template_rig_transfer.${_case} COMMAND iiActorObjectTemplateRigTransferTests ${_case})
    endforeach()
    add_executable(iiActorObjectTemplateRigYawRegistrationTests tests/TemplateRigYawRegistrationTest.cpp)
    target_link_libraries(iiActorObjectTemplateRigYawRegistrationTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectTemplateRigYawRegistrationTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case automatic validation shape_fit normal_ties adapt adapt_followers adapt_grounding adapt_shape_fit)
        add_test(NAME iiActorObject.template_rig_yaw.${_case} COMMAND iiActorObjectTemplateRigYawRegistrationTests ${_case})
    endforeach()
    if(IIACTOROBJECT_WITH_NSCHARACTER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
        add_test(NAME iiActorObject.template_rig_yaw.human COMMAND iiActorObjectTemplateRigYawRegistrationTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
    endif()
    add_executable(iiActorObjectPoseCorrectivesTests tests/PoseCorrectivesTest.cpp)
    target_link_libraries(iiActorObjectPoseCorrectivesTests PRIVATE iiActorObject::iiActorObject)
    add_executable(iiActorObjectAutomaticCorrectivesTests tests/AutomaticCorrectivesTest.cpp)
    target_link_libraries(iiActorObjectAutomaticCorrectivesTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectAutomaticCorrectivesTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case linear profiles pivot reference validation project)
        add_test(NAME iiActorObject.automatic_correctives.${_case} COMMAND iiActorObjectAutomaticCorrectivesTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.automatic_correctives.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyAutomaticCorrectives.py" "$<TARGET_FILE:iiActorObjectAutomaticCorrectivesTests>")
    if(IIACTOROBJECT_WITH_NSCHARACTER)
        add_executable(iiActorObjectHumanAutomaticCorrectivesTests tests/HumanAutomaticCorrectivesTest.cpp)
        target_link_libraries(iiActorObjectHumanAutomaticCorrectivesTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanAutomaticCorrectivesTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case plan validation interpolation)
            add_test(NAME iiActorObject.human_automatic_correctives.${_case} COMMAND iiActorObjectHumanAutomaticCorrectivesTests ${_case})
        endforeach()
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman/base.obj")
            add_test(NAME iiActorObject.human_automatic_correctives.human COMMAND iiActorObjectHumanAutomaticCorrectivesTests human
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
            add_test(NAME iiActorObject.human_automatic_correctives.human_dense COMMAND iiActorObjectHumanAutomaticCorrectivesTests human_dense
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
        endif()
    endif()
    set_target_properties(iiActorObjectPoseCorrectivesTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case evaluate component codec rebind project)
        add_test(NAME iiActorObject.pose_correctives.${_case} COMMAND iiActorObjectPoseCorrectivesTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.pose_correctives.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyPoseCorrectives.py" "$<TARGET_FILE:iiActorObjectPoseCorrectivesTests>"
        "${CMAKE_CURRENT_BINARY_DIR}")
    if(IIACTOROBJECT_WITH_NSCHARACTER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman/base.obj")
        add_test(NAME iiActorObject.pose_correctives.human COMMAND iiActorObjectPoseCorrectivesTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
    endif()
    add_executable(iiActorObjectPoseCorrectiveAnimationTests tests/PoseCorrectiveAnimationTest.cpp)
    target_link_libraries(iiActorObjectPoseCorrectiveAnimationTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectPoseCorrectiveAnimationTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case accuracy validation bound narrow multi conservative conservative_paths)
        add_test(NAME iiActorObject.pose_corrective_animation.${_case} COMMAND iiActorObjectPoseCorrectiveAnimationTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.pose_corrective_animation.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyPoseCorrectiveAnimation.py" "$<TARGET_FILE:iiActorObjectPoseCorrectiveAnimationTests>"
        "$<TARGET_FILE:iiActorObjectFbxAnimationUfbxTests>")
    if(IIACTOROBJECT_WITH_NSCHARACTER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman/base.obj")
        add_test(NAME iiActorObject.pose_corrective_animation.human COMMAND iiActorObjectPoseCorrectiveAnimationTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
    endif()
    add_executable(iiActorObjectTemplateAdaptTests tests/Object3DTemplateAdaptTest.cpp)
    target_link_libraries(iiActorObjectTemplateAdaptTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectTemplateAdaptTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case controls barycentric same_facing_identity permuted_identity permuted_followers shuffled_followers identity_collision_followers shuffled_faces identity_collision opposing_surfaces grounding followers validation ownership morphs morph_bind)
        add_test(NAME iiActorObject.template_adapt.${_case} COMMAND iiActorObjectTemplateAdaptTests ${_case})
    endforeach()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman/base.obj")
        add_test(NAME iiActorObject.template_adapt.human_identity COMMAND iiActorObjectTemplateAdaptTests human_identity
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
    endif()
    add_test(NAME iiActorObject.template_adapt.identity_formats_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyAdaptedIdentity.py" "$<TARGET_FILE:iiActorObjectTemplateAdaptTests>"
        "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME iiActorObject.template_adapt.format_fixture COMMAND iiActorObjectTemplateAdaptTests formats "${CMAKE_CURRENT_BINARY_DIR}/template-adapt-formats")
    set_tests_properties(iiActorObject.template_adapt.format_fixture PROPERTIES FIXTURES_SETUP iiActorObject_template_adapt_formats)
    add_test(NAME iiActorObject.template_adapt.format_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyTemplateAdapt.py" "${CMAKE_CURRENT_BINARY_DIR}/template-adapt-formats/Morph.fbx"
        "${CMAKE_CURRENT_BINARY_DIR}/template-adapt-formats/Morph.glb")
    set_tests_properties(iiActorObject.template_adapt.format_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_template_adapt_formats)
    add_test(NAME iiActorObject.template_adapt.follower_morph_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyFollowerMorphs.py" "${CMAKE_CURRENT_BINARY_DIR}/template-adapt-formats/MorphFollowers.fbx"
        "${CMAKE_CURRENT_BINARY_DIR}/template-adapt-formats/MorphFollowers.glb" body_rise OBJ,Garment 0.01)
    set_tests_properties(iiActorObject.template_adapt.follower_morph_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_template_adapt_formats)
    add_executable(iiActorObjectProjectTests tests/Object3DProjectTest.cpp)
    target_link_libraries(iiActorObjectProjectTests PRIVATE iiActorObject::iiActorObject)
    add_executable(iiActorObjectProjectAnimationTests tests/Object3DProjectAnimationTest.cpp)
    target_link_libraries(iiActorObjectProjectAnimationTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectProjectAnimationTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case roundtrip validation rebind)
        add_test(NAME iiActorObject.project_animation.${_case} COMMAND iiActorObjectProjectAnimationTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.project_animation.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectAnimation.py" "$<TARGET_FILE:iiActorObjectProjectAnimationTests>")
    if(IIACTOROBJECT_WITH_NSCHARACTER)
        add_test(NAME iiActorObject.project_animation.character COMMAND iiActorObjectProjectAnimationTests character)
    endif()
    add_test(NAME iiActorObject.external_edits.base_normal_project_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectFormat.py" "$<TARGET_FILE:iiActorObjectProjectTests>"
        "${CMAKE_CURRENT_BINARY_DIR}/base-normal/Fixture-v19.ii3dproject")
    set_tests_properties(iiActorObject.external_edits.base_normal_project_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_base_normal_project)
    add_test(NAME iiActorObject.project.legacy COMMAND iiActorObjectProjectTests legacy "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v1.ii3dproject")
    add_test(NAME iiActorObject.project.legacy_v2 COMMAND iiActorObjectProjectTests legacy "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v2.ii3dproject")
    add_test(NAME iiActorObject.project.legacy_v3 COMMAND iiActorObjectProjectTests legacy "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v3.ii3dproject")
    add_test(NAME iiActorObject.project.legacy_v4 COMMAND iiActorObjectProjectTests legacy "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v4.ii3dproject")
    add_test(NAME iiActorObject.project.legacy_v5 COMMAND iiActorObjectProjectTests legacy "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v5.ii3dproject")
    add_test(NAME iiActorObject.project.legacy_v6 COMMAND iiActorObjectProjectTests legacy "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v6.ii3dproject")
    add_test(NAME iiActorObject.project.legacy_v7 COMMAND iiActorObjectProjectTests legacy "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v7.ii3dproject")
    add_test(NAME iiActorObject.project.legacy_v8 COMMAND iiActorObjectProjectTests legacy "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/project/v8.ii3dproject")
    foreach(_case layers snapshots limits corruption triangulation atomic restore_validation runtime custom_driver morph_rig morph_missing_source morph_limits material_surface)
        add_test(NAME iiActorObject.project.${_case} COMMAND iiActorObjectProjectTests ${_case})
    endforeach()
    add_test(NAME iiActorObject.project.morph_fixture COMMAND iiActorObjectProjectTests morph_fixture
        "${CMAKE_CURRENT_BINARY_DIR}/project/Fixture-v21.ii3dproject")
    set_tests_properties(iiActorObject.project.morph_fixture PROPERTIES FIXTURES_SETUP iiActorObject_project_morph)
    add_test(NAME iiActorObject.project.morph_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyMorphProject.py" "$<TARGET_FILE:iiActorObjectProjectTests>"
        "${CMAKE_CURRENT_BINARY_DIR}/project/Fixture-v21.ii3dproject")
    set_tests_properties(iiActorObject.project.morph_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_project_morph)
    add_test(NAME iiActorObject.project.default_morph_fixture COMMAND iiActorObjectProjectTests default_morph_fixture
        "${CMAKE_CURRENT_BINARY_DIR}/project/Fixture-v23.ii3dproject")
    set_tests_properties(iiActorObject.project.default_morph_fixture PROPERTIES FIXTURES_SETUP iiActorObject_project_default_morph)
    add_test(NAME iiActorObject.project.default_morph_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectFormat.py" "$<TARGET_FILE:iiActorObjectProjectTests>"
        "${CMAKE_CURRENT_BINARY_DIR}/project/Fixture-v23.ii3dproject")
    set_tests_properties(iiActorObject.project.default_morph_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_project_default_morph)
    add_executable(iiActorObjectProjectIntegrityTests tests/Object3DProjectIntegrityTest.cpp)
    target_link_libraries(iiActorObjectProjectIntegrityTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectProjectIntegrityTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case uv_name uv_value uv_layout uv_material pose_order uv_duplicate_index uv_duplicate_set normal_uv normal_scale)
        add_test(NAME iiActorObject.project.integrity_${_case} COMMAND iiActorObjectProjectIntegrityTests ${_case}
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    endforeach()
    add_test(NAME iiActorObject.named_selection.fixture COMMAND iiActorObjectNamedPointSelectionTests fixture
        "${CMAKE_CURRENT_BINARY_DIR}/named-selection/Fixture-v17.ii3dproject")
    set_tests_properties(iiActorObject.named_selection.fixture PROPERTIES FIXTURES_SETUP iiActorObject_named_selection_project)
    add_test(NAME iiActorObject.named_selection.format_reference COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectFormat.py" "$<TARGET_FILE:iiActorObjectProjectTests>"
        "${CMAKE_CURRENT_BINARY_DIR}/named-selection/Fixture-v17.ii3dproject")
    set_tests_properties(iiActorObject.named_selection.format_reference PROPERTIES FIXTURES_REQUIRED iiActorObject_named_selection_project)
    add_executable(iiActorObjectMorphNormalTests tests/MorphNormalsTest.cpp)
    target_link_libraries(iiActorObjectMorphNormalTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case rendering skin layers fbx validation transformed_fbx)
        add_test(NAME iiActorObject.morph_normals.${_case} COMMAND iiActorObjectMorphNormalTests ${_case} "${CMAKE_CURRENT_BINARY_DIR}/morph-normals.glb")
    endforeach()
    set_tests_properties(iiActorObject.morph_normals.fbx PROPERTIES FIXTURES_SETUP iiActorObject_morph_normal_glb)
    add_executable(iiActorObjectFbxTopologyTests tests/Object3DFbxTopologyTest.cpp)
    target_link_libraries(iiActorObjectFbxTopologyTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectFbxTopologyTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case roundtrip materials large_point_ids foreign limits static metadata multi_uv multi_uv_hidden multi_uv_external multi_uv_validation)
        add_test(NAME iiActorObject.fbx_topology.${_case} COMMAND iiActorObjectFbxTopologyTests ${_case} "${CMAKE_CURRENT_BINARY_DIR}/fbx-topology")
    endforeach()
    add_test(NAME iiActorObject.fbx_topology.multi_uv_glb_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyMultiUvGlb.py" "$<TARGET_FILE:iiActorObjectFbxTopologyTests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_test(NAME iiActorObject.fbx_topology.multi_uv_material COMMAND iiActorObjectFbxTopologyTests multi_uv_material
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_test(NAME iiActorObject.fbx_topology.normal_maps COMMAND iiActorObjectFbxTopologyTests normal_maps
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_test(NAME iiActorObject.fbx_topology.normal_glb_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyNormalMapGlb.py" "$<TARGET_FILE:iiActorObjectFbxTopologyTests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_test(NAME iiActorObject.fbx_topology.normal_fbx_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyNormalMapFbx.py" "$<TARGET_FILE:iiActorObjectFbxTopologyTests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
    add_test(NAME iiActorObject.fbx_topology.normal_project_fixture COMMAND iiActorObjectFbxTopologyTests normal_project
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png" "${CMAKE_CURRENT_BINARY_DIR}/normal-map/Fixture-v18.ii3dproject")
    set_tests_properties(iiActorObject.fbx_topology.normal_project_fixture PROPERTIES FIXTURES_SETUP iiActorObject_normal_project)
    add_test(NAME iiActorObject.fbx_topology.normal_project_independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectFormat.py" "$<TARGET_FILE:iiActorObjectProjectTests>"
        "${CMAKE_CURRENT_BINARY_DIR}/normal-map/Fixture-v18.ii3dproject")
    set_tests_properties(iiActorObject.fbx_topology.normal_project_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_normal_project)
    set_tests_properties(iiActorObject.fbx_topology.roundtrip iiActorObject.fbx_topology.materials
        PROPERTIES FIXTURES_SETUP iiActorObject_fbx_topology)
    add_executable(iiActorObjectLayersTests tests/Object3DLayersTest.cpp)
    target_link_libraries(iiActorObjectLayersTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case parameters attributes joints order validation ownership render)
        add_test(NAME iiActorObject.layers.${_case} COMMAND iiActorObjectLayersTests ${_case})
    endforeach()
    add_executable(iiActorObjectTemplateTests tests/Object3DTemplateTest.cpp)
    target_link_libraries(iiActorObjectTemplateTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case source generation ownership validation)
        add_test(NAME iiActorObject.template.${_case} COMMAND iiActorObjectTemplateTests ${_case})
    endforeach()
    add_executable(iiActorObjectTopologyTests tests/MeshTopologyTest.cpp)
    target_link_libraries(iiActorObjectTopologyTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case layout seams editing weights morphs validation)
        add_test(NAME iiActorObject.topology.${_case} COMMAND iiActorObjectTopologyTests ${_case})
    endforeach()
    if(IIACTOROBJECT_WITH_DNA)
        add_executable(iiActorObjectDnaFixture tests/CreateDnaFixture.cpp $<TARGET_OBJECTS:iiActorObjectRigLogic>)
        target_include_directories(iiActorObjectDnaFixture SYSTEM PRIVATE src/third_party/openriglogic/include)
        target_compile_features(iiActorObjectDnaFixture PRIVATE cxx_std_23)
        add_executable(iiActorObjectDnaSourceVerifier tests/VerifyDnaSnapshot.cpp $<TARGET_OBJECTS:iiActorObjectRigLogic>)
        target_include_directories(iiActorObjectDnaSourceVerifier SYSTEM PRIVATE src/third_party/openriglogic/include)
        target_compile_features(iiActorObjectDnaSourceVerifier PRIVATE cxx_std_23)
        set_target_properties(iiActorObjectDnaSourceVerifier PROPERTIES CXX_EXTENSIONS OFF)
        set_target_properties(iiActorObjectDnaFixture PROPERTIES CXX_EXTENSIONS OFF)
        add_executable(iiActorObjectDnaTests tests/Object3DDnaTest.cpp)
        target_link_libraries(iiActorObjectDnaTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectDnaTests PROPERTIES CXX_EXTENSIONS OFF)
        add_executable(iiActorObjectDnaMlTests tests/DnaMlTest.cpp $<TARGET_OBJECTS:iiActorObjectRigLogic>)
        target_include_directories(iiActorObjectDnaMlTests SYSTEM PRIVATE src/third_party/openriglogic/include)
        target_compile_features(iiActorObjectDnaMlTests PRIVATE cxx_std_23)
        target_link_libraries(iiActorObjectDnaMlTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectDnaMlTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case active lod malformed graph graph_malformed)
            add_test(NAME iiActorObject.dna_ml.${_case} COMMAND iiActorObjectDnaMlTests ${_case}
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/dna/independent.dna")
        endforeach()
        add_executable(iiActorObjectDnaTwistTests tests/DnaTwistTest.cpp $<TARGET_OBJECTS:iiActorObjectRigLogic>)
        target_include_directories(iiActorObjectDnaTwistTests SYSTEM PRIVATE src/third_party/openriglogic/include)
        target_compile_features(iiActorObjectDnaTwistTests PRIVATE cxx_std_23)
        target_link_libraries(iiActorObjectDnaTwistTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectDnaTwistTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case valid malformed)
            add_test(NAME iiActorObject.dna_twist.${_case} COMMAND iiActorObjectDnaTwistTests ${_case}
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/dna/independent.dna")
        endforeach()
        add_executable(iiActorObjectDnaRbfTests tests/DnaRbfTest.cpp $<TARGET_OBJECTS:iiActorObjectRigLogic>)
        target_include_directories(iiActorObjectDnaRbfTests SYSTEM PRIVATE src/third_party/openriglogic/include)
        target_compile_features(iiActorObjectDnaRbfTests PRIVATE cxx_std_23)
        target_link_libraries(iiActorObjectDnaRbfTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectDnaRbfTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case valid malformed)
            add_test(NAME iiActorObject.dna_rbf.${_case} COMMAND iiActorObjectDnaRbfTests ${_case}
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/dna/independent.dna")
        endforeach()
        foreach(_case import controls rebind uv_loss progressive_project)
            add_test(NAME iiActorObject.dna_resource.${_case} COMMAND iiActorObjectDnaTests ${_case}
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/dna")
        endforeach()
        add_test(NAME iiActorObject.dna.fixture COMMAND iiActorObjectDnaFixture "${CMAKE_CURRENT_BINARY_DIR}/dna")
        set_tests_properties(iiActorObject.dna.fixture PROPERTIES FIXTURES_SETUP iiActorObject_dna)
        add_test(NAME iiActorObject.rig_snapshot.dna COMMAND iiActorObjectRigSnapshotTests dna "${CMAKE_CURRENT_BINARY_DIR}/dna")
        set_tests_properties(iiActorObject.rig_snapshot.dna PROPERTIES FIXTURES_REQUIRED iiActorObject_dna)
        add_test(NAME iiActorObject.rig_samples.dna COMMAND iiActorObjectRigSamplesTests dna "${CMAKE_CURRENT_BINARY_DIR}/dna")
        set_tests_properties(iiActorObject.rig_samples.dna PROPERTIES FIXTURES_REQUIRED iiActorObject_dna)
        if(IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION AND TARGET iiActorObjectRigSamples)
            add_test(NAME iiActorObject.rig_samples.cli COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyRigSamplesCli.py" "$<TARGET_FILE:iiActorObjectRigSamples>"
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/dna/independent.dna" "${CMAKE_CURRENT_BINARY_DIR}/rig-samples-cli")
        endif()
        foreach(_case import controls axes limits rebind interchange source_roundtrip source_edited source_layered source_fbx source_grouped_fbx source_lods source_validation source_normals source_project source_reference)
            add_test(NAME iiActorObject.dna.${_case} COMMAND iiActorObjectDnaTests ${_case} "${CMAKE_CURRENT_BINARY_DIR}/dna")
            set_tests_properties(iiActorObject.dna.${_case} PROPERTIES FIXTURES_REQUIRED iiActorObject_dna)
        endforeach()
        set_tests_properties(iiActorObject.dna.source_roundtrip PROPERTIES FIXTURES_SETUP iiActorObject_dna_roundtrip)
        set_tests_properties(iiActorObject.dna.source_edited PROPERTIES FIXTURES_SETUP iiActorObject_dna_edited)
        set_tests_properties(iiActorObject.dna.source_layered PROPERTIES FIXTURES_SETUP iiActorObject_dna_layered)
        set_tests_properties(iiActorObject.dna.source_fbx PROPERTIES FIXTURES_SETUP iiActorObject_dna_fbx)
        set_tests_properties(iiActorObject.dna.source_grouped_fbx PROPERTIES FIXTURES_SETUP iiActorObject_dna_grouped_fbx)
        set_tests_properties(iiActorObject.dna.source_lods PROPERTIES FIXTURES_SETUP iiActorObject_dna_lods)
        set_tests_properties(iiActorObject.dna.source_validation PROPERTIES FIXTURES_SETUP iiActorObject_dna_validation)
        set_tests_properties(iiActorObject.dna.source_project PROPERTIES FIXTURES_SETUP iiActorObject_dna_project)
        foreach(_pair "unedited|roundtrip" "edited|edited" "layered|layered" "fbx|fbx" "grouped_fbx|grouped_fbx" "lods|lods" "validation|validation" "project|project")
            string(REPLACE "|" ";" _parts "${_pair}")
            list(GET _parts 0 _mode)
            list(GET _parts 1 _fixture)
            add_test(NAME iiActorObject.dna.native_${_mode} COMMAND iiActorObjectDnaSourceVerifier "${CMAKE_CURRENT_BINARY_DIR}/dna" ${_mode})
            set_tests_properties(iiActorObject.dna.native_${_mode} PROPERTIES FIXTURES_REQUIRED "iiActorObject_dna;iiActorObject_dna_${_fixture}")
        endforeach()
        if(IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION AND TARGET iiActorObjectDnaCharacter)
            add_test(NAME iiActorObject.dna.example COMMAND iiActorObjectDnaCharacter
                "${CMAKE_CURRENT_BINARY_DIR}/dna/independent.dna" "${CMAKE_CURRENT_BINARY_DIR}/dna/IndependentFace.glb")
            set_tests_properties(iiActorObject.dna.example PROPERTIES FIXTURES_REQUIRED iiActorObject_dna)
            add_test(NAME iiActorObject.dna.example_source COMMAND iiActorObjectDnaCharacter
                "${CMAKE_CURRENT_BINARY_DIR}/dna/independent.dna" "${CMAKE_CURRENT_BINARY_DIR}/dna/IndependentFace.dna")
            set_tests_properties(iiActorObject.dna.example_source PROPERTIES FIXTURES_REQUIRED iiActorObject_dna FIXTURES_SETUP iiActorObject_dna_cli)
            add_test(NAME iiActorObject.dna.native_example COMMAND iiActorObjectDnaSourceVerifier "${CMAKE_CURRENT_BINARY_DIR}/dna" example)
            set_tests_properties(iiActorObject.dna.native_example PROPERTIES FIXTURES_REQUIRED iiActorObject_dna_cli)
        endif()
    endif()
    add_executable(iiActorObjectRigTests tests/Object3DRigTest.cpp)
    target_link_libraries(iiActorObjectRigTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case ownership evaluation clone validation)
        add_test(NAME iiActorObject.rig.${_case} COMMAND iiActorObjectRigTests ${_case})
    endforeach()
    add_executable(iiActorObjectEditorRigTests tests/Object3DEditorRigTest.cpp)
    target_link_libraries(iiActorObjectEditorRigTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectEditorRigTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case follow identity atomic real_programs lifetime)
        add_test(NAME iiActorObject.editor_rig.${_case} COMMAND iiActorObjectEditorRigTests ${_case})
    endforeach()
    add_executable(iiActorObjectInstanceTransferTests tests/Object3DInstanceTransferTest.cpp)
    add_executable(iiActorObjectInstanceTransferHeadersTests
        tests/InstanceTransferCoreHeaderTest.cpp tests/InstanceTransferEditorHeaderTest.cpp
        tests/InstanceTransferCompatibilityHeaderTest.cpp)
    target_link_libraries(iiActorObjectInstanceTransferHeadersTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectInstanceTransferHeadersTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.instance_transfer_headers.standalone COMMAND iiActorObjectInstanceTransferHeadersTests)
    target_link_libraries(iiActorObjectInstanceTransferTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectInstanceTransferTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case animated world safeguards deformation_bind scene topology synchronized)
        add_test(NAME iiActorObject.instance_transfer.${_case} COMMAND iiActorObjectInstanceTransferTests ${_case})
    endforeach()
    if(NOT BUILD_SHARED_LIBS)
        # Exercise public RTTI across a static archive and a consumer that
        # builds its own code with hidden visibility.
        add_executable(iiActorObjectInstanceTransferHiddenConsumerTests tests/Object3DInstanceTransferTest.cpp)
        target_link_libraries(iiActorObjectInstanceTransferHiddenConsumerTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectInstanceTransferHiddenConsumerTests PROPERTIES
            CXX_EXTENSIONS OFF CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
        foreach(_case animated safeguards)
            add_test(NAME iiActorObject.instance_transfer_hidden_consumer.${_case}
                COMMAND iiActorObjectInstanceTransferHiddenConsumerTests ${_case})
        endforeach()
    endif()
    add_executable(iiActorObject3DEngineTests tests/Object3DEngineTest.cpp)
    target_link_libraries(iiActorObject3DEngineTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case generic collection surface limits)
        add_test(NAME iiActorObject.object3d.${_case} COMMAND iiActorObject3DEngineTests ${_case})
    endforeach()
    if(IIACTOROBJECT_WITH_NSCHARACTER)
        add_executable(iiActorObjectCharacterHeadersTests tests/CharacterTemplateCoreHeaderTest.cpp tests/MakeHumanTemplateHeaderTest.cpp tests/HumanAccessoriesHeaderTest.cpp)
        target_link_libraries(iiActorObjectCharacterHeadersTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectCharacterHeadersTests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME iiActorObject.character_headers.standalone COMMAND iiActorObjectCharacterHeadersTests)
        add_executable(iiActorObjectHumanAccessoriesTests tests/HumanAccessoriesTest.cpp)
        target_link_libraries(iiActorObjectHumanAccessoriesTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanAccessoriesTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case catalog human alternate editing editing_large batch visibility removal addition composition retained_slot rig_follow instance_follow progressive_split progressive_sparse grounded failures)
            add_test(NAME iiActorObject.human_accessories.${_case} COMMAND iiActorObjectHumanAccessoriesTests ${_case}
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanWardrobe")
        endforeach()
        add_test(NAME iiActorObject.human_accessories.source_integrity COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanAccessoryAssets.py" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanWardrobe")
        add_test(NAME iiActorObject.human_accessories.independent_formats COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanAccessoryFormats.py" "$<TARGET_FILE:iiActorObjectHumanAccessoriesTests>"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanWardrobe" "${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME iiActorObject.human_accessories.switch_formats COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanAccessoryFormats.py" "$<TARGET_FILE:iiActorObjectHumanAccessoriesTests>"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanWardrobe" "${CMAKE_CURRENT_BINARY_DIR}" --switch)
        add_test(NAME iiActorObject.human_accessories.remove_formats COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanAccessoryFormats.py" "$<TARGET_FILE:iiActorObjectHumanAccessoriesTests>"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanWardrobe" "${CMAKE_CURRENT_BINARY_DIR}" --remove)
        add_test(NAME iiActorObject.human_accessories.add_formats COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanAccessoryFormats.py" "$<TARGET_FILE:iiActorObjectHumanAccessoriesTests>"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanWardrobe" "${CMAKE_CURRENT_BINARY_DIR}" --add)
        add_test(NAME iiActorObject.human_accessories.mixed_formats COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanAccessoryFormats.py" "$<TARGET_FILE:iiActorObjectHumanAccessoriesTests>"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanWardrobe" "${CMAKE_CURRENT_BINARY_DIR}" --mixed)
        add_executable(iiActorObjectHumanSkinTests tests/HumanSkinTest.cpp)
        target_link_libraries(iiActorObjectHumanSkinTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanSkinTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case catalog editing template failures)
            add_test(NAME iiActorObject.human_skin.${_case} COMMAND iiActorObjectHumanSkinTests ${_case}
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanSkin")
        endforeach()
        add_test(NAME iiActorObject.human_skin.independent_formats COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanSkinFormats.py" "$<TARGET_FILE:iiActorObjectHumanSkinTests>"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanSkin"
            "${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME iiActorObject.human_skin.source_integrity COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanSkinAssets.py" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanSkin")
        add_executable(iiActorObjectMeshTangentHumanTests tests/MeshTangentHumanTest.cpp)
        target_link_libraries(iiActorObjectMeshTangentHumanTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectMeshTangentHumanTests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME iiActorObject.mesh_tangents.human COMMAND iiActorObjectMeshTangentHumanTests
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/blue.png"
            "${CMAKE_CURRENT_BINARY_DIR}/mesh-tangents-human.glb"
            "${CMAKE_CURRENT_BINARY_DIR}/mesh-tangents-human.fbx"
            "${CMAKE_CURRENT_BINARY_DIR}/mesh-tangents-human-roundtrip.fbx")
        set_tests_properties(iiActorObject.mesh_tangents.human PROPERTIES FIXTURES_SETUP iiActorObject_mesh_tangents_human)
        add_test(NAME iiActorObject.mesh_tangents.human_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanTangentsGlb.py" "${CMAKE_CURRENT_BINARY_DIR}/mesh-tangents-human.glb")
        set_tests_properties(iiActorObject.mesh_tangents.human_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_mesh_tangents_human)
        add_test(NAME iiActorObject.fbx_tangents.human_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanFbxTangents.py" "${CMAKE_CURRENT_BINARY_DIR}/mesh-tangents-human.fbx")
        set_tests_properties(iiActorObject.fbx_tangents.human_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_mesh_tangents_human)
        add_test(NAME iiActorObject.fbx_tangents.human_roundtrip_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanFbxTangentRoundtrip.py"
            "${CMAKE_CURRENT_BINARY_DIR}/mesh-tangents-human.fbx" "${CMAKE_CURRENT_BINARY_DIR}/mesh-tangents-human-roundtrip.fbx")
        set_tests_properties(iiActorObject.fbx_tangents.human_roundtrip_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_mesh_tangents_human)
        add_executable(iiActorObjectHumanPoseDirectionTests tests/HumanPoseDirectionsTest.cpp)
        target_link_libraries(iiActorObjectHumanPoseDirectionTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanPoseDirectionTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case basic atomic lifetime partial validation frames frames_atomic ordered ordered_atomic reference reference_atomic reference_lifetime)
            add_test(NAME iiActorObject.human_pose.${_case} COMMAND iiActorObjectHumanPoseDirectionTests ${_case})
        endforeach()
        add_executable(iiActorObjectHumanPosePlannerTests tests/HumanPosePlannerTest.cpp)
        target_link_libraries(iiActorObjectHumanPosePlannerTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanPosePlannerTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case basic axes finger4 partial failures lifetime toe_group)
            add_test(NAME iiActorObject.human_pose_plan.${_case} COMMAND iiActorObjectHumanPosePlannerTests ${_case})
        endforeach()
        add_executable(iiActorObjectHumanPoseRetargetTests tests/HumanPoseRetargetTest.cpp)
        target_link_libraries(iiActorObjectHumanPoseRetargetTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanPoseRetargetTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case automatic partial lifetime)
            add_test(NAME iiActorObject.human_pose_retarget.${_case} COMMAND iiActorObjectHumanPoseRetargetTests ${_case})
        endforeach()
        add_executable(iiActorObjectHumanCreatorConventionTests tests/HumanCreatorConventionTest.cpp)
        target_link_libraries(iiActorObjectHumanCreatorConventionTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanCreatorConventionTests PROPERTIES CXX_EXTENSIONS OFF)
        add_executable(iiActorObjectHumanInterchangeConventionTests tests/HumanInterchangeConventionTest.cpp)
        target_link_libraries(iiActorObjectHumanInterchangeConventionTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanInterchangeConventionTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case g5 blender blender_sanitized invalid naming automatic ambiguous detection_failures detection_lifetime)
            add_test(NAME iiActorObject.human_interchange_convention.${_case} COMMAND iiActorObjectHumanInterchangeConventionTests ${_case})
        endforeach()
        foreach(_shape adult child)
            add_test(NAME iiActorObject.human_interchange_convention.detect_asset.${_shape}
                COMMAND iiActorObjectHumanInterchangeConventionTests detect_asset
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/rig-sampling/${_shape}.fbx")
            set_tests_properties(iiActorObject.human_interchange_convention.detect_asset.${_shape} PROPERTIES TIMEOUT 300)
        endforeach()
        set(IIACTOROBJECT_CC5_PROFILE_DIR "" CACHE PATH "Optional user-owned CC5 characterization profile directory for independent comparison")
        if(IIACTOROBJECT_CC5_PROFILE_DIR)
            add_test(NAME iiActorObject.human_interchange_convention.cc5_profile_reference COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyCc5Characterization.py" "$<TARGET_FILE:iiActorObjectHumanInterchangeConventionTests>"
                "${IIACTOROBJECT_CC5_PROFILE_DIR}/CC_G5Character.3dxProfile" "${IIACTOROBJECT_CC5_PROFILE_DIR}/Blender_Meta_Rig.3dxProfile")
        endif()
        foreach(_case mapping partial hierarchy naming naming_invalid)
            add_test(NAME iiActorObject.human_creator_convention.${_case} COMMAND iiActorObjectHumanCreatorConventionTests ${_case})
        endforeach()
        foreach(_shape adult child)
            add_test(NAME iiActorObject.human_creator_convention.naming_asset.${_shape} COMMAND iiActorObjectHumanCreatorConventionTests naming_asset
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/rig-sampling/${_shape}.fbx")
            set_tests_properties(iiActorObject.human_creator_convention.naming_asset.${_shape} PROPERTIES TIMEOUT 300)
        endforeach()
        add_test(NAME iiActorObject.human_pose_retarget.assets COMMAND iiActorObjectHumanPoseRetargetTests assets
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/rig-sampling/adult.fbx"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/rig-sampling/child.fbx")
        set_tests_properties(iiActorObject.human_pose_retarget.assets PROPERTIES TIMEOUT 300)
        foreach(_shape adult child)
            add_test(NAME iiActorObject.human_pose_plan.asset.${_shape} COMMAND iiActorObjectHumanPosePlannerTests human
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/rig-sampling/${_shape}.fbx")
            set_tests_properties(iiActorObject.human_pose_plan.asset.${_shape} PROPERTIES TIMEOUT 300)
        endforeach()
        add_executable(iiActorObjectHumanReferenceTemplateTests tests/HumanReferenceTemplateTest.cpp)
        target_link_libraries(iiActorObjectHumanReferenceTemplateTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanReferenceTemplateTests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME iiActorObject.human_reference.template COMMAND iiActorObjectHumanReferenceTemplateTests
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanFace"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanMacro" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanExpressions"
            "${CMAKE_CURRENT_BINARY_DIR}/reference-template")
        set_tests_properties(iiActorObject.human_reference.template PROPERTIES FIXTURES_SETUP iiActorObject_reference_template TIMEOUT 1200)
        add_executable(iiActorObjectHumanCharacterizationTests tests/HumanoidCharacterizationTest.cpp)
        target_link_libraries(iiActorObjectHumanCharacterizationTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanCharacterizationTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case basic missing validation hierarchy semantics encoding limits lifetime conventions renamed)
            add_test(NAME iiActorObject.characterization.${_case} COMMAND iiActorObjectHumanCharacterizationTests ${_case})
        endforeach()
        foreach(_shape adult child)
            add_test(NAME iiActorObject.characterization.human.${_shape} COMMAND iiActorObjectHumanCharacterizationTests human
                "${CMAKE_CURRENT_BINARY_DIR}/face-pose/${_shape}/PoseHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/characterization/${_shape}.3dxProfile")
            set_tests_properties(iiActorObject.characterization.human.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_face_pose FIXTURES_SETUP iiActorObject_characterization_${_shape})
        endforeach()
        add_executable(iiActorObjectAutomaticFittingHumanTests tests/AutomaticFittingHumanTest.cpp)
        target_link_libraries(iiActorObjectAutomaticFittingHumanTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.automatic_fitting.human COMMAND iiActorObjectAutomaticFittingHumanTests
            "${CMAKE_CURRENT_BINARY_DIR}/surface-fitting/FittedHuman.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/automatic-fitting")
        set_tests_properties(iiActorObject.automatic_fitting.human PROPERTIES FIXTURES_REQUIRED iiActorObject_surface_fitting FIXTURES_SETUP iiActorObject_automatic_fitting)
        add_executable(iiActorObjectSurfaceFairingHumanTests tests/SurfaceFairingHumanTest.cpp)
        target_link_libraries(iiActorObjectSurfaceFairingHumanTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.surface_fairing.human COMMAND iiActorObjectSurfaceFairingHumanTests
            "${CMAKE_CURRENT_BINARY_DIR}/automatic-fitting/AutomaticHuman.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing")
        set_tests_properties(iiActorObject.surface_fairing.human PROPERTIES FIXTURES_REQUIRED iiActorObject_automatic_fitting FIXTURES_SETUP iiActorObject_surface_fairing_human TIMEOUT 3600)
        foreach(_shape adult child)
            add_test(NAME iiActorObject.deformation_snapshot.human.${_shape} COMMAND iiActorObjectDeformationSnapshotTests human
                "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing/${_shape}/FittedHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/deformation-snapshot/human/${_shape}")
            set_tests_properties(iiActorObject.deformation_snapshot.human.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_surface_fairing_human FIXTURES_SETUP iiActorObject_deformation_snapshot_${_shape})
            add_test(NAME iiActorObject.deformation_snapshot.reference.${_shape} COMMAND iiActorObjectPoseSnapshotReference
                "${CMAKE_CURRENT_BINARY_DIR}/deformation-snapshot/human/${_shape}/PoseSnapshot.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/deformation-snapshot/human/${_shape}/Expected.json")
            set_tests_properties(iiActorObject.deformation_snapshot.reference.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_deformation_snapshot_${_shape} FIXTURES_SETUP iiActorObject_deformation_snapshot_reference_${_shape})
            add_test(NAME iiActorObject.pose_snapshot.human.${_shape} COMMAND iiActorObjectPoseSnapshotTests human
                "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing/${_shape}/FittedHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/pose-snapshot/human/${_shape}")
            set_tests_properties(iiActorObject.pose_snapshot.human.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_surface_fairing_human FIXTURES_SETUP iiActorObject_pose_snapshot_${_shape})
            add_test(NAME iiActorObject.pose_snapshot.reference.${_shape} COMMAND iiActorObjectPoseSnapshotReference
                "${CMAKE_CURRENT_BINARY_DIR}/pose-snapshot/human/${_shape}/PoseSnapshot.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/pose-snapshot/human/${_shape}/Expected.json")
            set_tests_properties(iiActorObject.pose_snapshot.reference.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_pose_snapshot_${_shape} FIXTURES_SETUP iiActorObject_pose_snapshot_reference_${_shape})
            add_test(NAME iiActorObject.human_pose.human.${_shape} COMMAND iiActorObjectHumanPoseDirectionTests human
                "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing/${_shape}/FittedHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/human-pose/human/${_shape}")
            set_tests_properties(iiActorObject.human_pose.human.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_surface_fairing_human FIXTURES_SETUP iiActorObject_human_pose_${_shape})
            add_test(NAME iiActorObject.human_pose.reference.${_shape} COMMAND iiActorObjectPoseSnapshotReference
                "${CMAKE_CURRENT_BINARY_DIR}/human-pose/human/${_shape}/PoseSnapshot.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/human-pose/human/${_shape}/Expected.json")
            set_tests_properties(iiActorObject.human_pose.reference.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_human_pose_${_shape} FIXTURES_SETUP iiActorObject_human_pose_reference_${_shape})
            add_test(NAME iiActorObject.human_reference.human.${_shape} COMMAND iiActorObjectHumanPoseDirectionTests reference_human
                "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing/${_shape}/FittedHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/reference-pose/human/${_shape}")
            set_tests_properties(iiActorObject.human_reference.human.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_surface_fairing_human FIXTURES_SETUP iiActorObject_human_reference_${_shape})
            add_test(NAME iiActorObject.human_reference.reference.${_shape} COMMAND iiActorObjectPoseSnapshotReference
                "${CMAKE_CURRENT_BINARY_DIR}/reference-pose/human/${_shape}/PoseSnapshot.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/reference-pose/human/${_shape}/Expected.json")
            set_tests_properties(iiActorObject.human_reference.reference.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_human_reference_${_shape} FIXTURES_SETUP iiActorObject_human_reference_reference_${_shape})
            add_test(NAME iiActorObject.human_reference.template_reference.${_shape} COMMAND iiActorObjectPoseSnapshotReference
                "${CMAKE_CURRENT_BINARY_DIR}/reference-template/human/${_shape}/PoseSnapshot.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/reference-template/human/${_shape}/Expected.json")
            set_tests_properties(iiActorObject.human_reference.template_reference.${_shape} PROPERTIES
                FIXTURES_REQUIRED iiActorObject_reference_template FIXTURES_SETUP iiActorObject_reference_template_reference_${_shape})
        endforeach()
        if(IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION AND TARGET iiActorObjectPoseSnapshot)
            add_test(NAME iiActorObject.human_pose.example COMMAND iiActorObjectPoseSnapshot
                "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing/adult/FittedHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/human-pose/Example.fbx"
                upperarm01.L lowerarm01.L 1 0 0 lowerarm01.L wrist.L 1 0 0
                upperarm01.R lowerarm01.R -1 0 0 lowerarm01.R wrist.R -1 0 0)
            set_tests_properties(iiActorObject.human_pose.example PROPERTIES FIXTURES_REQUIRED iiActorObject_surface_fairing_human)
        endif()
        add_executable(iiActorObjectSurfaceFittingHumanTests tests/SurfaceFittingHumanTest.cpp)
        target_link_libraries(iiActorObjectSurfaceFittingHumanTests PRIVATE iiActorObject::iiActorObject)
        add_dependencies(iiActorObjectAutomaticFittingHumanTests iiActorObjectSurfaceFittingHumanTests iiActorObjectAutomaticFittingTests iiActorObjectProjectTests)
        add_executable(iiActorObjectEvaluatedExternalHumanTests tests/EvaluatedExternalHumanTest.cpp)
        target_link_libraries(iiActorObjectEvaluatedExternalHumanTests PRIVATE iiActorObject::iiActorObject)
        add_dependencies(iiActorObjectEvaluatedExternalHumanTests iiActorObjectAutomaticFittingHumanTests)
        add_test(NAME iiActorObject.surface_collision.human COMMAND iiActorObjectSurfaceFittingHumanTests
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanMacro"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanFace" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanExpressions"
            "${CMAKE_CURRENT_BINARY_DIR}/surface-fitting")
        set_tests_properties(iiActorObject.surface_collision.human PROPERTIES FIXTURES_SETUP iiActorObject_surface_fitting)
        add_executable(iiActorObjectMeshSymmetryHumanTests tests/MeshSymmetryHumanTest.cpp)
        target_link_libraries(iiActorObjectMeshSymmetryHumanTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.mesh_symmetry.human COMMAND iiActorObjectMeshSymmetryHumanTests
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanMacro"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanFace" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanExpressions"
            "${CMAKE_CURRENT_BINARY_DIR}/mesh-symmetry")
        set_tests_properties(iiActorObject.mesh_symmetry.human PROPERTIES FIXTURES_SETUP iiActorObject_mesh_symmetry)
        add_executable(iiActorObjectFacePoseHumanTests tests/FacePoseHumanTest.cpp)
        target_link_libraries(iiActorObjectFacePoseHumanTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.face_pose.human COMMAND iiActorObjectFacePoseHumanTests
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanMacro"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanFace" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanExpressions"
            "${CMAKE_CURRENT_BINARY_DIR}/face-pose")
        set_tests_properties(iiActorObject.face_pose.human PROPERTIES FIXTURES_SETUP iiActorObject_face_pose)
        add_executable(iiActorObjectHumanMorphologyTests tests/HumanMorphologyTest.cpp)
        target_link_libraries(iiActorObjectHumanMorphologyTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.morphology.parameters COMMAND iiActorObjectHumanMorphologyTests parameters)
        add_test(NAME iiActorObject.morphology.human COMMAND iiActorObjectHumanMorphologyTests
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanMacro"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanFace" "${CMAKE_CURRENT_BINARY_DIR}/morphology")
        set_tests_properties(iiActorObject.morphology.human PROPERTIES FIXTURES_SETUP iiActorObject_morphology)
        add_executable(iiActorObjectFaceAssetsHumanTests tests/FaceAssetsHumanTest.cpp)
        target_link_libraries(iiActorObjectFaceAssetsHumanTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.face_assets.human COMMAND iiActorObjectFaceAssetsHumanTests "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanFace" "${CMAKE_CURRENT_BINARY_DIR}/face-assets")
        set_tests_properties(iiActorObject.face_assets.human PROPERTIES FIXTURES_SETUP iiActorObject_face_assets)
        add_executable(iiActorObjectTemplateControlsHumanTests tests/TemplateControlsHumanTest.cpp)
        target_link_libraries(iiActorObjectTemplateControlsHumanTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.template_controls.human COMMAND iiActorObjectTemplateControlsHumanTests "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_BINARY_DIR}/template-controls")
        set_tests_properties(iiActorObject.template_controls.human PROPERTIES FIXTURES_SETUP iiActorObject_template_controls)
        add_executable(iiActorObjectExternalEditsHumanTests tests/Object3DExternalEditsHumanTest.cpp)
        target_link_libraries(iiActorObjectExternalEditsHumanTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.external_edits.human_source COMMAND iiActorObjectExternalEditsHumanTests prepare "${CMAKE_CURRENT_BINARY_DIR}/skeleton-editing-human/EditedHuman.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/external-edits")
        set_tests_properties(iiActorObject.external_edits.human_source PROPERTIES FIXTURES_REQUIRED iiActorObject_skeleton_human FIXTURES_SETUP iiActorObject_external_source)
        add_executable(iiActorObjectSkeletonEditingHumanTests tests/SkeletonEditingHumanTest.cpp)
        target_link_libraries(iiActorObjectSkeletonEditingHumanTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.skeleton_editing.human COMMAND iiActorObjectSkeletonEditingHumanTests "${CMAKE_CURRENT_BINARY_DIR}/painting/Painted.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/skeleton-editing-human")
        set_tests_properties(iiActorObject.skeleton_editing.human PROPERTIES FIXTURES_REQUIRED iiActorObject_painting FIXTURES_SETUP iiActorObject_skeleton_human)
        add_executable(iiActorObjectPaintingHumanTests tests/MeshPaintingHumanTest.cpp)
        target_link_libraries(iiActorObjectPaintingHumanTests PRIVATE iiActorObject::iiActorObject)
        add_test(NAME iiActorObject.painting.human COMMAND iiActorObjectPaintingHumanTests "${CMAKE_CURRENT_BINARY_DIR}/skin-surface/ClothedHuman.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/painting")
        set_tests_properties(iiActorObject.painting.human PROPERTIES FIXTURES_REQUIRED iiActorObject_skin_surface FIXTURES_SETUP iiActorObject_painting)
        add_test(NAME iiActorObject.skin_surface.human COMMAND iiActorObjectSkinSurfaceTests human "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_BINARY_DIR}/skin-surface")
        set_tests_properties(iiActorObject.skin_surface.human PROPERTIES FIXTURES_SETUP iiActorObject_skin_surface)
        add_test(NAME iiActorObject.skin_geodesic.human COMMAND iiActorObjectSkinGeodesicTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_BINARY_DIR}/skin-geodesic")
        set_tests_properties(iiActorObject.skin_geodesic.human PROPERTIES FIXTURES_SETUP iiActorObject_skin_geodesic_human)
        add_test(NAME iiActorObject.skin_geodesic.human_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanGeodesicSkin.py"
            "${CMAKE_CURRENT_BINARY_DIR}/skin-geodesic/Human.fbx" "${CMAKE_CURRENT_BINARY_DIR}/skin-geodesic/Human.glb")
        set_tests_properties(iiActorObject.skin_geodesic.human_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_skin_geodesic_human)
        add_test(NAME iiActorObject.skin_geodesic.generated COMMAND iiActorObjectSkinGeodesicTests generated)
        add_test(NAME iiActorObject.skeleton_envelopes.human COMMAND iiActorObjectSkeletonEnvelopeTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_BINARY_DIR}/skeleton-envelopes")
        set_tests_properties(iiActorObject.skeleton_envelopes.human PROPERTIES FIXTURES_SETUP iiActorObject_skeleton_envelopes_human)
        add_test(NAME iiActorObject.skeleton_envelopes.human_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanGeodesicSkin.py"
            "${CMAKE_CURRENT_BINARY_DIR}/skeleton-envelopes/Human.fbx" "${CMAKE_CURRENT_BINARY_DIR}/skeleton-envelopes/Human.glb")
        set_tests_properties(iiActorObject.skeleton_envelopes.human_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_skeleton_envelopes_human)
        add_test(NAME iiActorObject.template_rig_transfer.human COMMAND iiActorObjectTemplateRigTransferTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_BINARY_DIR}/template-rig-transfer")
        set_tests_properties(iiActorObject.template_rig_transfer.human PROPERTIES FIXTURES_SETUP iiActorObject_template_rig_transfer_human)
        add_test(NAME iiActorObject.template_rig_transfer.human_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyTemplateRigTransfer.py"
            "${CMAKE_CURRENT_BINARY_DIR}/template-rig-transfer/Human.fbx" "${CMAKE_CURRENT_BINARY_DIR}/template-rig-transfer/Human.glb"
            "${CMAKE_CURRENT_BINARY_DIR}/template-rig-transfer/Reference.fbx" "${CMAKE_CURRENT_BINARY_DIR}/template-rig-transfer/Reference.glb")
        set_tests_properties(iiActorObject.template_rig_transfer.human_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_template_rig_transfer_human)
        add_test(NAME iiActorObject.template_rig_transfer.human_automatic COMMAND iiActorObjectTemplateRigTransferTests human_automatic
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_BINARY_DIR}/template-rig-automatic")
        set_tests_properties(iiActorObject.template_rig_transfer.human_automatic PROPERTIES FIXTURES_SETUP iiActorObject_template_rig_automatic_human)
        add_test(NAME iiActorObject.template_adapt.human COMMAND iiActorObjectTemplateAdaptTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_BINARY_DIR}/template-adapt")
        set_tests_properties(iiActorObject.template_adapt.human PROPERTIES FIXTURES_SETUP iiActorObject_template_adapt_human)
        add_test(NAME iiActorObject.template_adapt.human_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyAutomaticRigTransfer.py"
            "${CMAKE_CURRENT_BINARY_DIR}/template-adapt/Human.fbx" "${CMAKE_CURRENT_BINARY_DIR}/template-adapt/Human.glb")
        set_tests_properties(iiActorObject.template_adapt.human_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_template_adapt_human)
        add_test(NAME iiActorObject.template_adapt.face_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyAdaptedFace.py"
            "${CMAKE_CURRENT_BINARY_DIR}/template-adapt/FaceHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/template-adapt/FaceHuman.glb")
        set_tests_properties(iiActorObject.template_adapt.face_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_template_adapt_human)
        add_test(NAME iiActorObject.template_adapt.face_morph_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyFollowerMorphs.py"
            "${CMAKE_CURRENT_BINARY_DIR}/template-adapt/FaceHumanMorph.fbx" "${CMAKE_CURRENT_BINARY_DIR}/template-adapt/FaceHumanMorph.glb"
            head_width OBJ,Eyes,Teeth,Tongue)
        set_tests_properties(iiActorObject.template_adapt.face_morph_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_template_adapt_human)
        add_test(NAME iiActorObject.template_rig_transfer.human_automatic_independent COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyAutomaticRigTransfer.py"
            "${CMAKE_CURRENT_BINARY_DIR}/template-rig-automatic/Human.fbx" "${CMAKE_CURRENT_BINARY_DIR}/template-rig-automatic/Human.glb")
        set_tests_properties(iiActorObject.template_rig_transfer.human_automatic_independent PROPERTIES FIXTURES_REQUIRED iiActorObject_template_rig_automatic_human)
        add_test(NAME iiActorObject.project.human COMMAND iiActorObjectProjectTests human "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_BINARY_DIR}/project")
        add_executable(iiActorObjectHumanTemplateTests tests/CharacterTemplateTest.cpp)
        target_link_libraries(iiActorObjectHumanTemplateTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanTemplateTests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME iiActorObject.human.generation COMMAND iiActorObjectHumanTemplateTests generation
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman" "${CMAKE_CURRENT_BINARY_DIR}/human")
        set_tests_properties(iiActorObject.human.generation PROPERTIES FIXTURES_SETUP iiActorObject_human)
        add_test(NAME iiActorObject.human.lifetime COMMAND iiActorObjectHumanTemplateTests lifetime
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
        add_test(NAME iiActorObject.human.grouped_external COMMAND iiActorObjectHumanTemplateTests grouped_external
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
        add_test(NAME iiActorObject.object3d.ownership COMMAND iiActorObject3DEngineTests ownership)
    add_executable(iiActorObjectAuthoringTests tests/NSCharacterTest.cpp)
    target_link_libraries(iiActorObjectAuthoringTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectAuthoringTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case asset validation normals binding generation topology edits sculpt morphs weights joints history glb fbx fbx_io fbx_transforms influences)
        add_test(NAME iiActorObject.authoring.${_case} COMMAND iiActorObjectAuthoringTests ${_case})
    endforeach()
    add_executable(iiActorObjectProceduralOralTests tests/ProceduralOralTest.cpp)
    target_link_libraries(iiActorObjectProceduralOralTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectProceduralOralTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.authoring.oral_fixture COMMAND iiActorObjectProceduralOralTests "${CMAKE_CURRENT_BINARY_DIR}/procedural-oral")
    set_tests_properties(iiActorObject.authoring.oral_fixture PROPERTIES FIXTURES_SETUP iiActorObject_procedural_oral)
    add_test(NAME iiActorObject.authoring.oral_formats COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProceduralOral.py" "${CMAKE_CURRENT_BINARY_DIR}/procedural-oral")
    set_tests_properties(iiActorObject.authoring.oral_formats PROPERTIES FIXTURES_REQUIRED iiActorObject_procedural_oral)
    option(IIACTOROBJECT_VALIDATE_CHARACTER_FORMATS "Independently validate character interchange with Python" OFF)
    if(IIACTOROBJECT_VALIDATE_CHARACTER_FORMATS)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        if(IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION AND TARGET iiActorObjectPoseSnapshot)
            add_test(NAME iiActorObject.human_pose.example_safety COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyPoseSnapshotCli.py" "$<TARGET_FILE:iiActorObjectPoseSnapshot>"
                "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing/adult/FittedHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/pose-cli")
            set_tests_properties(iiActorObject.human_pose.example_safety PROPERTIES FIXTURES_REQUIRED iiActorObject_surface_fairing_human)
        endif()
        add_test(NAME iiActorObject.surface_fairing.format_reference COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectFormat.py" "$<TARGET_FILE:iiActorObjectProjectTests>" "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing/Fixture-v12.ii3dproject")
        set_tests_properties(iiActorObject.surface_fairing.format_reference PROPERTIES FIXTURES_REQUIRED iiActorObject_fairing_project)
        add_test(NAME iiActorObject.automatic_fitting.format_reference COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectFormat.py" "$<TARGET_FILE:iiActorObjectProjectTests>"
            "${CMAKE_CURRENT_BINARY_DIR}/automatic-fitting/Fixture-v10.ii3dproject")
        set_tests_properties(iiActorObject.automatic_fitting.format_reference PROPERTIES FIXTURES_REQUIRED iiActorObject_automatic_fitting_project)
        add_test(NAME iiActorObject.project.fixture COMMAND iiActorObjectProjectTests fixture "${CMAKE_CURRENT_BINARY_DIR}/project/Fixture.ii3dproject")
        set_tests_properties(iiActorObject.project.fixture PROPERTIES FIXTURES_SETUP iiActorObject_project)
        add_test(NAME iiActorObject.project.format_reference COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectFormat.py" "$<TARGET_FILE:iiActorObjectProjectTests>" "${CMAKE_CURRENT_BINARY_DIR}/project/Fixture.ii3dproject")
        set_tests_properties(iiActorObject.project.format_reference PROPERTIES FIXTURES_REQUIRED iiActorObject_project)
        add_test(NAME iiActorObject.reference_pose.format_reference COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectFormat.py" "$<TARGET_FILE:iiActorObjectProjectTests>" "${CMAKE_CURRENT_BINARY_DIR}/reference-pose/Fixture-v13.ii3dproject")
        set_tests_properties(iiActorObject.reference_pose.format_reference PROPERTIES FIXTURES_REQUIRED iiActorObject_reference_pose)
        add_test(NAME iiActorObject.reference_pose.ordered_format_reference COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyProjectFormat.py" "$<TARGET_FILE:iiActorObjectProjectTests>" "${CMAKE_CURRENT_BINARY_DIR}/reference-pose/Fixture-v15.ii3dproject")
        set_tests_properties(iiActorObject.reference_pose.ordered_format_reference PROPERTIES FIXTURES_REQUIRED iiActorObject_ordered_reference_pose)
        add_test(NAME iiActorObject.morph_normals.glb_reference COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyMorphNormalsGlb.py" "${CMAKE_CURRENT_BINARY_DIR}/morph-normals.glb")
        set_tests_properties(iiActorObject.morph_normals.glb_reference PROPERTIES FIXTURES_REQUIRED iiActorObject_morph_normal_glb)
        add_test(NAME iiActorObject.authoring.glb_fixture COMMAND iiActorObjectAuthoringTests glb "${CMAKE_CURRENT_BINARY_DIR}/character/contract.glb")
        set_tests_properties(iiActorObject.authoring.glb_fixture PROPERTIES FIXTURES_SETUP iiActorObject_character_glb)
        add_test(NAME iiActorObject.authoring.fbx_fixture COMMAND iiActorObjectAuthoringTests fbx "${CMAKE_CURRENT_BINARY_DIR}/character/contract.fbx")
        set_tests_properties(iiActorObject.authoring.fbx_fixture PROPERTIES FIXTURES_SETUP iiActorObject_character_fbx)
        add_test(NAME iiActorObject.authoring.influences_fixture COMMAND iiActorObjectAuthoringTests influences "${CMAKE_CURRENT_BINARY_DIR}/character/influences.glb")
        set_tests_properties(iiActorObject.authoring.influences_fixture PROPERTIES FIXTURES_SETUP iiActorObject_character_influences)
        add_test(NAME iiActorObject.authoring.glb_reference COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyCharacterGlb.py" "${CMAKE_CURRENT_BINARY_DIR}/character/contract.glb")
        set_tests_properties(iiActorObject.authoring.glb_reference PROPERTIES FIXTURES_REQUIRED iiActorObject_character_glb)
        set(IIACTOROBJECT_BLENDER_EXECUTABLE "" CACHE FILEPATH "Optional Blender executable for actual import/edit/deformation tests")
        if(IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION AND IIACTOROBJECT_BLENDER_EXECUTABLE AND IIACTOROBJECT_BUILD_EXAMPLE)
            foreach(_pair "deformation_snapshot|deformation-snapshot" "pose_snapshot|pose-snapshot" "human_pose|human-pose" "human_reference|reference-pose")
                string(REPLACE "|" ";" _parts "${_pair}")
                list(GET _parts 0 _fixture)
                list(GET _parts 1 _directory)
                add_test(NAME iiActorObject.${_fixture}.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                    --background --factory-startup --threads 2 --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyPoseSnapshotBlender.py"
                    -- "${CMAKE_CURRENT_BINARY_DIR}/${_directory}")
                set_tests_properties(iiActorObject.${_fixture}.blender PROPERTIES
                    FIXTURES_REQUIRED "iiActorObject_${_fixture}_reference_adult;iiActorObject_${_fixture}_reference_child")
            endforeach()
            add_test(NAME iiActorObject.human_reference.template_blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --threads 2 --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyPoseSnapshotBlender.py"
                -- "${CMAKE_CURRENT_BINARY_DIR}/reference-template" 4)
            set_tests_properties(iiActorObject.human_reference.template_blender PROPERTIES
                FIXTURES_REQUIRED "iiActorObject_reference_template_reference_adult;iiActorObject_reference_template_reference_child")
            add_test(NAME iiActorObject.characterization.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --threads 2 --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanoidCharacterizationBlender.py"
                -- "${CMAKE_CURRENT_BINARY_DIR}/characterization/BlenderVerification.json"
                adult "${CMAKE_CURRENT_BINARY_DIR}/face-pose/adult/PoseHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/characterization/adult.3dxProfile"
                child "${CMAKE_CURRENT_BINARY_DIR}/face-pose/child/PoseHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/characterization/child.3dxProfile")
            set_tests_properties(iiActorObject.characterization.blender PROPERTIES
                FIXTURES_REQUIRED "iiActorObject_characterization_adult;iiActorObject_characterization_child")
            add_test(NAME iiActorObject.surface_fairing.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --threads 2 --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifySurfaceFittingBlender.py"
                -- "${CMAKE_CURRENT_BINARY_DIR}/surface-fairing")
            set_tests_properties(iiActorObject.surface_fairing.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_surface_fairing_human)
            add_test(NAME iiActorObject.evaluated_external.prepare COMMAND iiActorObjectEvaluatedExternalHumanTests prepare
                "${CMAKE_CURRENT_BINARY_DIR}/automatic-fitting/AutomaticHuman.ii3dproject" "${CMAKE_CURRENT_BINARY_DIR}/evaluated-external")
            set_tests_properties(iiActorObject.evaluated_external.prepare PROPERTIES FIXTURES_REQUIRED iiActorObject_automatic_fitting FIXTURES_SETUP iiActorObject_evaluated_external_source)
            add_test(NAME iiActorObject.evaluated_external.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --threads 2 --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyEvaluatedExternalBlender.py"
                -- "${CMAKE_CURRENT_SOURCE_DIR}/example/BlenderExternalEdits.py" "${CMAKE_CURRENT_BINARY_DIR}/evaluated-external")
            set_tests_properties(iiActorObject.evaluated_external.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_evaluated_external_source FIXTURES_SETUP iiActorObject_evaluated_external_app)
            add_test(NAME iiActorObject.evaluated_external.consume COMMAND iiActorObjectEvaluatedExternalHumanTests consume "${CMAKE_CURRENT_BINARY_DIR}/evaluated-external")
            set_tests_properties(iiActorObject.evaluated_external.consume PROPERTIES FIXTURES_REQUIRED iiActorObject_evaluated_external_app FIXTURES_SETUP iiActorObject_evaluated_external_merged)
            add_test(NAME iiActorObject.evaluated_external.merged_blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --threads 2 --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyEvaluatedMergedBlender.py"
                -- "${CMAKE_CURRENT_SOURCE_DIR}/example/BlenderExternalEdits.py" "${CMAKE_CURRENT_BINARY_DIR}/evaluated-external")
            set_tests_properties(iiActorObject.evaluated_external.merged_blender PROPERTIES FIXTURES_REQUIRED iiActorObject_evaluated_external_merged)
            add_test(NAME iiActorObject.automatic_fitting.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifySurfaceFittingBlender.py"
                -- "${CMAKE_CURRENT_BINARY_DIR}/automatic-fitting")
            set_tests_properties(iiActorObject.automatic_fitting.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_automatic_fitting)
            add_test(NAME iiActorObject.surface_collision.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifySurfaceFittingBlender.py"
                -- "${CMAKE_CURRENT_BINARY_DIR}/surface-fitting")
            set_tests_properties(iiActorObject.surface_collision.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_surface_fitting)
            add_test(NAME iiActorObject.mesh_symmetry.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyMeshSymmetryBlender.py"
                -- "${CMAKE_CURRENT_BINARY_DIR}/mesh-symmetry")
            set_tests_properties(iiActorObject.mesh_symmetry.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_mesh_symmetry)
            add_test(NAME iiActorObject.images.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyImagesBlender.py" -- "${CMAKE_CURRENT_BINARY_DIR}/images" "${CMAKE_CURRENT_SOURCE_DIR}/resources/fixtures/images/checker.png")
            set_tests_properties(iiActorObject.images.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_images)
            add_test(NAME iiActorObject.face_assets.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyFaceAssetsBlender.py" -- "${CMAKE_CURRENT_BINARY_DIR}/face-assets" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanFace")
            set_tests_properties(iiActorObject.face_assets.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_face_assets)
            add_test(NAME iiActorObject.morphology.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyMacroHumanBlender.py"
                -- "${CMAKE_CURRENT_BINARY_DIR}/morphology" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanFace")
            set_tests_properties(iiActorObject.morphology.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_morphology)
            add_test(NAME iiActorObject.face_pose.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyFacePoseBlender.py"
                -- "${CMAKE_CURRENT_BINARY_DIR}/face-pose" "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHumanExpressions")
            set_tests_properties(iiActorObject.face_pose.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_face_pose)


            add_test(NAME iiActorObject.template_controls.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyTemplateControlsBlender.py" -- "${CMAKE_CURRENT_BINARY_DIR}/template-controls")
            set_tests_properties(iiActorObject.template_controls.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_template_controls)
            add_test(NAME iiActorObject.external_edits.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyExternalEditsBlender.py" -- "${CMAKE_CURRENT_SOURCE_DIR}/example/BlenderExternalEdits.py" "${CMAKE_CURRENT_BINARY_DIR}/external-edits")
            set_tests_properties(iiActorObject.external_edits.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_external_source FIXTURES_SETUP iiActorObject_external_edited)
            add_test(NAME iiActorObject.external_edits.blender_reimport COMMAND iiActorObjectExternalEditsHumanTests consume "${CMAKE_CURRENT_BINARY_DIR}/external-edits")
            set_tests_properties(iiActorObject.external_edits.blender_reimport PROPERTIES FIXTURES_REQUIRED iiActorObject_external_edited)
            add_test(NAME iiActorObject.skeleton_editing.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifySkeletonEditingBlender.py" -- "${CMAKE_CURRENT_BINARY_DIR}/skeleton-editing" "${CMAKE_CURRENT_BINARY_DIR}/skeleton-editing-human")
            set_tests_properties(iiActorObject.skeleton_editing.blender PROPERTIES FIXTURES_REQUIRED "iiActorObject_skeleton_editing;iiActorObject_skeleton_human")
            add_test(NAME iiActorObject.painting.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyPaintingBlender.py" -- "${CMAKE_CURRENT_BINARY_DIR}/painting")
            set_tests_properties(iiActorObject.painting.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_painting)
            add_test(NAME iiActorObject.skin_surface.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifySurfaceBlender.py" -- "${CMAKE_CURRENT_BINARY_DIR}/skin-surface")
            set_tests_properties(iiActorObject.skin_surface.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_skin_surface)
            add_test(NAME iiActorObject.fbx_topology.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyFbxTopologyBlender.py" -- "${CMAKE_CURRENT_BINARY_DIR}/fbx-topology")
            set_tests_properties(iiActorObject.fbx_topology.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_fbx_topology)
            add_test(NAME iiActorObject.human.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}"
                --background --factory-startup --python-exit-code 1 --python
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyHumanTemplateBlender.py" -- "${CMAKE_CURRENT_BINARY_DIR}/human")
            set_tests_properties(iiActorObject.human.blender PROPERTIES FIXTURES_REQUIRED iiActorObject_human)
            add_test(NAME iiActorObject.authoring.generated_fixture COMMAND iiActorObjectCharacterAuthoring "${CMAKE_CURRENT_BINARY_DIR}/character/GeneratedHuman.glb" 96)
            set_tests_properties(iiActorObject.authoring.generated_fixture PROPERTIES FIXTURES_SETUP iiActorObject_character_generated)
            add_test(NAME iiActorObject.authoring.generated_fbx_fixture COMMAND iiActorObjectCharacterAuthoring "${CMAKE_CURRENT_BINARY_DIR}/character/GeneratedHuman.fbx" 96)
            set_tests_properties(iiActorObject.authoring.generated_fbx_fixture PROPERTIES FIXTURES_SETUP iiActorObject_character_generated_fbx)
            add_test(NAME iiActorObject.object3d.static_glb_fixture COMMAND iiActorObjectCore3D "${CMAKE_CURRENT_BINARY_DIR}/object3d/CoreSphere.glb")
            set_tests_properties(iiActorObject.object3d.static_glb_fixture PROPERTIES FIXTURES_SETUP iiActorObject_static_glb)
            add_test(NAME iiActorObject.object3d.static_fbx_fixture COMMAND iiActorObjectCore3D "${CMAKE_CURRENT_BINARY_DIR}/object3d/CoreSphere.fbx")
            set_tests_properties(iiActorObject.object3d.static_fbx_fixture PROPERTIES FIXTURES_SETUP iiActorObject_static_fbx)
            add_test(NAME iiActorObject.authoring.blender COMMAND "${IIACTOROBJECT_BLENDER_EXECUTABLE}" --background --factory-startup --python-exit-code 1 --python "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyCharacterBlender.py" -- "${CMAKE_CURRENT_BINARY_DIR}/character/contract.glb" "${CMAKE_CURRENT_BINARY_DIR}/character/GeneratedHuman.glb" "${CMAKE_CURRENT_BINARY_DIR}/character/contract.fbx" "${CMAKE_CURRENT_BINARY_DIR}/character/GeneratedHuman.fbx" "${CMAKE_CURRENT_BINARY_DIR}/character/influences.glb" "${CMAKE_CURRENT_BINARY_DIR}/object3d/CoreSphere.glb" "${CMAKE_CURRENT_BINARY_DIR}/object3d/CoreSphere.fbx" "${CMAKE_CURRENT_BINARY_DIR}/character/blender")
            set_tests_properties(iiActorObject.authoring.blender PROPERTIES FIXTURES_REQUIRED "iiActorObject_character_glb;iiActorObject_character_fbx;iiActorObject_character_influences;iiActorObject_character_generated;iiActorObject_character_generated_fbx;iiActorObject_static_glb;iiActorObject_static_fbx" TIMEOUT 180)
            set_tests_properties(iiActorObject.authoring.blender PROPERTIES FIXTURES_SETUP iiActorObject_blender_roundtrip)
            add_test(NAME iiActorObject.authoring.blender_native_reimport COMMAND iiActorObjectAuthoringTests fbx_foreign "${CMAKE_CURRENT_BINARY_DIR}/character/blender/GeneratedHuman-roundtrip.fbx")
            set_tests_properties(iiActorObject.authoring.blender_native_reimport PROPERTIES FIXTURES_REQUIRED iiActorObject_blender_roundtrip)
        endif()
    endif()
    endif()
    add_executable(iiActorObjectCharacterTests tests/CharacterTest.cpp)
    target_link_libraries(iiActorObjectCharacterTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectCharacterTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case bones dirty dynamic_bind inverse animation clip_validation player skin normals mesh_validation component fk ik ik_sweep face face_alias modular character clock_error stress)
        add_test(NAME iiActorObject.character.${_case} COMMAND iiActorObjectCharacterTests ${_case})
    endforeach()
    add_executable(iiActorObjectDualQuaternionTests tests/DualQuaternionSkinningTest.cpp)
    target_link_libraries(iiActorObjectDualQuaternionTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectDualQuaternionTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case rotations normals scale_morph validation component topology asset profile editing guards)
        add_test(NAME iiActorObject.dual_quaternion.${_case} COMMAND iiActorObjectDualQuaternionTests ${_case})
    endforeach()
    if(IIACTOROBJECT_WITH_NSCHARACTER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
        add_test(NAME iiActorObject.dual_quaternion.human COMMAND iiActorObjectDualQuaternionTests human
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/MakeHuman")
        set_tests_properties(iiActorObject.dual_quaternion.human PROPERTIES TIMEOUT 1200)
    endif()
    add_executable(iiActorObjectDualQuaternionReferenceTests tests/DualQuaternionReferenceTest.cpp src/third_party/ufbx/ufbx.cpp)
    target_link_libraries(iiActorObjectDualQuaternionReferenceTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectDualQuaternionReferenceTests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.dual_quaternion.ufbx_reference COMMAND iiActorObjectDualQuaternionReferenceTests)
    add_executable(iiActorObjectTests tests/ActorTest.cpp)
    target_link_libraries(iiActorObjectTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case identity properties transform scene components lifecycle callbacks validation)
        add_test(NAME iiActorObject.${_case} COMMAND iiActorObjectTests ${_case})
    endforeach()
    add_executable(iiActorObjectUsdTests tests/UsdTest.cpp)
    target_link_libraries(iiActorObjectUsdTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectUsdTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case matrix layer hierarchy references validation)
        add_test(NAME iiActorObject.usd.${_case} COMMAND iiActorObjectUsdTests ${_case})
    endforeach()
    set(_usd_fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/usd-fixture")
    add_test(NAME iiActorObject.usd.fixture COMMAND iiActorObjectUsdTests fixture "${_usd_fixture_dir}")
    set_tests_properties(iiActorObject.usd.fixture PROPERTIES FIXTURES_SETUP iiActorObject_usd_fixture)
    option(IIACTOROBJECT_VALIDATE_OPENUSD "Validate generated USD with Python pxr" OFF)
    set(IIACTOROBJECT_USD_PYTHONPATH "" CACHE PATH "Directory containing the validation-only pxr module")
    if(IIACTOROBJECT_VALIDATE_OPENUSD)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(_usd_python_command "${CMAKE_COMMAND}" -E env "PYTHONDONTWRITEBYTECODE=1")
        if(IIACTOROBJECT_USD_PYTHONPATH)
            list(APPEND _usd_python_command "PYTHONPATH=${IIACTOROBJECT_USD_PYTHONPATH}")
        endif()
        list(APPEND _usd_python_command "${Python3_EXECUTABLE}" -B)
        execute_process(COMMAND ${_usd_python_command} -c "from pxr import Usd, UsdGeom, Sdf"
            RESULT_VARIABLE _usd_probe ERROR_VARIABLE _usd_probe_error)
        if(NOT _usd_probe EQUAL 0)
            message(FATAL_ERROR "OpenUSD validation requested but pxr is unavailable: ${_usd_probe_error}")
        endif()
        add_test(NAME iiActorObject.usd.openusd COMMAND ${_usd_python_command}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyUsd.py" "${_usd_fixture_dir}")
        set_tests_properties(iiActorObject.usd.openusd PROPERTIES FIXTURES_REQUIRED iiActorObject_usd_fixture)
    endif()
endif()

set(_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/iiActorObject")
configure_package_config_file(
    cmake/iiActorObjectConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectConfig.cmake"
    INSTALL_DESTINATION "${_package_dir}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)
install(TARGETS iiActorObject EXPORT iiActorObjectTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES iiActorObject.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES
    src/Interchange/GLB/GlbDocument.h
    src/Interchange/GLB/GlbImageResolver.h
    src/Interchange/GLB/Object3DGlbImportOptions.h
    src/Interchange/GLB/GlbAnimationSource.h
    src/Interchange/GLB/GlbAnimationPose.h
    src/Interchange/GLB/GlbAnimationBake.h
    src/Rig/Object3DAnimation.h
    src/Project/Object3DProjectAnimation.h
    src/Project/Object3DProjectAnimationData.h
    src/Runtime/Actor.h
    src/Runtime/ActorComponent.h
    src/Runtime/SceneComponent.h
    src/Core/Transform.h
    src/Core/BoneIndex.h
    src/Core/MeshData.h
    src/Interchange/Usd.h
    src/Rig/Skeleton.h
    src/Rig/Animation.h
    src/Rig/SkeletalMesh.h
    src/Rig/SkeletalMeshCore.h
    src/Runtime/SkeletalMeshComponent.h
    src/Rig/DualQuaternionSkinning.h
    src/Rig/CharacterRig.h
    src/Runtime/Character.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/Export.h" "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/Features.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject)
install(FILES
    src/Geometry/MeshPointMask.h
    src/Geometry/MeshPointMaskValidation.h
    src/Core/Object3DEditLayerData.h
    src/Core/Object3DPoseProgramData.h
    src/Project/Object3DProjectOptions.h
    src/Core/Object3DTemplateMorphData.h
    src/Core/Object3DTemplateTypes.h
    src/Geometry/SurfaceFitSettings.h
    src/Runtime/Object3DAsset.h
    src/Core/Object3DData.h
    src/Geometry/Object3DMesh.h
    src/Core/Object3DMaterial.h
    src/Core/Object3DImage.h
    src/Rig/Object3DBvh.h
    src/Rig/Object3DPoseRig.h
    src/Rig/Object3DPoseCorrectives.h
    src/Rig/Object3DAutomaticCorrectives.h
    src/Rig/Object3DPoseCorrectiveAnimation.h
    src/Rig/Object3DPoseMorphs.h
    src/Rig/Object3DPoseSnapshot.h
    src/Rig/Object3DRigSnapshot.h
    src/Rig/Object3DRigSamples.h
    src/Rig/RigControlSpace.h
    src/Rig/RigSampleCatalog.h
    src/Rig/RigSampleCatalogCore.h
    src/Interchange/RigSampleCatalogExport.h
    src/Rig/PoseAiming.h
    src/Rig/PoseFrames.h
    src/Rig/PoseReference.h
    src/Rig/PoseRetarget.h
    src/Rig/AnimationRetarget.h
    src/Interchange/Object3DProxy.h
    src/Authoring/Object3DTemplate.h
    src/Authoring/Object3DTemplateMorphs.h
    src/Rig/Object3DProgressiveMorph.h
    src/Interchange/GLB/Object3DGlbAnimationOptions.h
    src/Core/Object3DProgressiveMorphData.h
    src/Rig/Object3DProgressiveMorphCore.h
    src/Interchange/Object3DProgressiveMorphInterchange.h
    src/Authoring/Object3DTemplateRigTransfer.h
    src/Authoring/Object3DTemplateAdapt.h
    src/Interchange/Object3DObj.h
    src/Runtime/Object3DEngine.h
    src/Authoring/Object3DEditor.h
    src/Authoring/Object3DEditorRig.h
    src/Runtime/Object3DInstanceTransfer.h
    src/Runtime/Object3DInstanceTransferCore.h
    src/Authoring/Object3DEditorInstanceTransfer.h
    src/Runtime/Object3DJointIdentity.h
    src/Rig/Object3DRig.h
    src/Interchange/Object3DDna.h
    src/Interchange/Object3DInterchange.h
    src/Interchange/FBX/FbxImageResolver.h
    src/Project/Object3DProject.h
    src/Geometry/MeshGeometry.h
    src/Geometry/MeshGeometryCore.h
    src/Geometry/MeshTangents.h
    src/Geometry/MeshTopology.h
    src/Geometry/MeshTopologyCore.h
    src/Geometry/MeshPrimitives.h
    src/Rig/SkinBinding.h
    src/Geometry/MeshSurface.h
    src/Geometry/SurfaceQuery.h
    src/Geometry/SurfaceCollider.h
    src/Geometry/MeshCollider.h
    src/Geometry/SurfaceFitting.h
    src/Geometry/SurfaceFittingRules.h
    src/Geometry/SurfaceBinding.h
    src/Geometry/SurfaceBindingCore.h
    src/Rig/SurfaceAttachment.h
    src/Geometry/MeshEditing.h
    src/Geometry/MeshScreenSelection.h
    src/Geometry/MeshScreenSelectionCore.h
    src/Interchange/MeshScreenSelectionAsset.h
    src/Geometry/MeshMaskProjection.h
    src/Geometry/MeshMaskProjectionCore.h
    src/Interchange/MeshMaskProjectionAsset.h
    src/Geometry/MeshSymmetry.h
    src/Geometry/MorphSymmetry.h
    src/Rig/SkinPainting.h
    src/Rig/SkeletonEditing.h
    src/Rig/SkeletonScale.h
    src/Core/Object3DExternalIdentity.h
    src/Interchange/Object3DExternalEdits.h
    src/Geometry/ImplicitSurface.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject)
if(IIACTOROBJECT_WITH_NSCHARACTER)
install(FILES
    src/NSCharacter/NSCharacter.h
    src/NSCharacter/CharacterAsset.h
    src/NSCharacter/CharacterTemplate.h
    src/NSCharacter/CharacterTemplateCore.h
    src/NSCharacter/HumanAccessories.h
    src/NSCharacter/MakeHumanTemplate.h
    src/NSCharacter/HumanMorphology.h
    src/NSCharacter/HumanSkin.h
    src/NSCharacter/HumanoidCharacterization.h
    src/NSCharacter/HumanPoseDirections.h
    src/NSCharacter/HumanPosePlanner.h
    src/NSCharacter/HumanAutomaticCorrectives.h
    src/NSCharacter/HumanPoseRetarget.h
    src/NSCharacter/HumanJointNaming.h
    src/NSCharacter/RigSamples.h
    src/NSCharacter/AutoRig.h
    src/NSCharacter/HumanoidGenerator.h
    src/NSCharacter/CharacterEditor.h
    src/NSCharacter/CharacterInterchange.h
    src/NSCharacter/DnaCharacter.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject/NSCharacter)
endif()
if(IIACTOROBJECT_WITH_NSCHARACTER)
    install(DIRECTORY resources/MakeHuman DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/templates")
    install(DIRECTORY resources/MakeHumanFace DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/templates")
    install(DIRECTORY resources/MakeHumanMacro DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/templates")
    install(DIRECTORY resources/MakeHumanExpressions DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/templates")
    install(DIRECTORY resources/MakeHumanSkin DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/templates")
    install(DIRECTORY resources/MakeHumanWardrobe DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/templates")
endif()
install(EXPORT iiActorObjectTargets NAMESPACE iiActorObject:: DESTINATION "${_package_dir}")
if(IIACTOROBJECT_WITH_VISUALCODEC)
    install(EXPORT iiActorObjectVisualCodecTargets NAMESPACE iiActorObject:: DESTINATION "${_package_dir}")
endif()
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectConfigVersion.cmake"
    DESTINATION "${_package_dir}"
)
install(FILES README.md DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject")
install(FILES src/third_party/ufbx/LICENSE src/third_party/ufbx/PROVENANCE.md DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/licenses/ufbx")
install(FILES src/third_party/mikktspace/LICENSE src/third_party/mikktspace/PROVENANCE.md DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/licenses/mikktspace")
if(IIACTOROBJECT_WITH_DNA)
    install(FILES src/third_party/openriglogic/LICENSE src/third_party/openriglogic/PROVENANCE.md
        src/third_party/openriglogic/UPSTREAM-SHA256.json
        DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/licenses/openriglogic")
endif()
install(DIRECTORY docs/ DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/docs")
install(FILES resources/architecture/ResponsibilityMap.json
    DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/resources/architecture")
install(FILES example/BlenderExternalEdits.py DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/integration")
