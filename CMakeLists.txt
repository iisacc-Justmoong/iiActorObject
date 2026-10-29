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

add_library(iiActorObject src/iiActorObject/Actor.cpp
    src/iiActorObject/GlbDocument.cpp
    src/iiActorObject/GlbAnimationSource.cpp
    src/iiActorObject/GlbAnimationPose.cpp
    src/iiActorObject/GlbAnimationBake.cpp
    src/iiActorObject/Object3DAnimation.cpp
    src/iiActorObject/Object3DProjectAnimation.cpp
    src/iiActorObject/Object3DNames.cpp
    src/iiActorObject/RigSampleCatalog.cpp
    src/iiActorObject/Object3DImage.cpp
    src/iiActorObject/Object3DProxy.cpp
    src/iiActorObject/Object3DBvh.cpp
    src/iiActorObject/Object3DPoseRig.cpp
    src/iiActorObject/Object3DPoseCorrectives.cpp
    src/iiActorObject/Object3DAutomaticCorrectives.cpp
    src/iiActorObject/Object3DPoseCorrectiveAnimation.cpp
    src/iiActorObject/Object3DPoseMorphs.cpp
    src/iiActorObject/Object3DPoseSnapshot.cpp
    src/iiActorObject/Object3DRigSnapshot.cpp
    src/iiActorObject/Object3DRigSamples.cpp
    src/iiActorObject/PoseAiming.cpp
    src/iiActorObject/PoseFrames.cpp
    src/iiActorObject/PoseReference.cpp
    src/iiActorObject/PoseRetarget.cpp
    src/iiActorObject/AnimationRetarget.cpp
    src/iiActorObject/ActorComponent.cpp src/iiActorObject/SceneComponent.cpp src/iiActorObject/Transform.cpp src/iiActorObject/Usd.cpp
    src/iiActorObject/Skeleton.cpp src/iiActorObject/Animation.cpp src/iiActorObject/SkeletalMesh.cpp
    src/iiActorObject/DualQuaternionSkinning.cpp src/iiActorObject/SkinningPalette.cpp
    src/iiActorObject/CharacterRig.cpp src/iiActorObject/Character.cpp
    src/iiActorObject/Object3DAsset.cpp
    src/iiActorObject/Object3DTemplate.cpp
    src/iiActorObject/Object3DTemplateMorphs.cpp
    src/iiActorObject/Object3DProgressiveMorph.cpp
    src/iiActorObject/Object3DProgressiveMorphAnimation.cpp
    src/iiActorObject/Object3DTemplateRigTransfer.cpp
    src/iiActorObject/Object3DTemplateRigProjection.cpp
    src/iiActorObject/Object3DPointCorrespondence.cpp
    src/iiActorObject/Object3DTemplateShapeFit.cpp
    src/iiActorObject/Object3DTemplateAdapt.cpp
    src/iiActorObject/Object3DTemplateAdaptFollowers.cpp
    src/iiActorObject/Object3DObj.cpp
    src/iiActorObject/Object3DEngine.cpp
    src/iiActorObject/Object3DEditor.cpp
    src/iiActorObject/Object3DSelections.cpp
    src/iiActorObject/Object3DAdjustment.cpp
    src/iiActorObject/Object3DEditLayers.cpp
    src/iiActorObject/Object3DTemplateSurfaceSwap.cpp
    src/iiActorObject/Object3DProject.cpp
    src/iiActorObject/Object3DRig.cpp
    src/iiActorObject/Object3DEditorRig.cpp
    src/iiActorObject/Object3DInstanceTransfer.cpp
    src/iiActorObject/Object3DDna.cpp
    src/iiActorObject/MeshGeometry.cpp
    src/iiActorObject/MeshTangents.cpp
    src/iiActorObject/MeshTopology.cpp
    src/iiActorObject/MeshPrimitives.cpp
    src/iiActorObject/ImplicitSurface.cpp
    src/iiActorObject/SkinBinding.cpp
    src/iiActorObject/SkinEnvelopeInference.cpp
    src/iiActorObject/SkinSurfaceBinding.cpp
    src/iiActorObject/MeshSurface.cpp
    src/iiActorObject/MeshSurfaceCorrespondence.cpp
    src/iiActorObject/MeshCollider.cpp
    src/iiActorObject/SurfaceFitting.cpp
    src/iiActorObject/SurfaceFittingRules.cpp
    src/iiActorObject/SurfaceBinding.cpp
    src/iiActorObject/MeshEditing.cpp
    src/iiActorObject/MeshScreenSelection.cpp
    src/iiActorObject/MeshMaskProjection.cpp
    src/iiActorObject/MeshSymmetry.cpp
    src/iiActorObject/MorphSymmetry.cpp
    src/iiActorObject/SkinPainting.cpp
    src/iiActorObject/SkeletonEditing.cpp
    src/iiActorObject/SkeletonScale.cpp
    src/iiActorObject/Object3DExternalEdits.cpp
    src/iiActorObject/Object3DMorphChannels.cpp
    src/iiActorObject/Object3DGlbImport.cpp
    src/iiActorObject/Object3DGlbExternalEdits.cpp
    src/iiActorObject/Object3DGlbPointIdentity.cpp
    src/iiActorObject/Object3DGlbTangentPlan.cpp
    src/iiActorObject/Object3DInterchange.cpp
    src/iiActorObject/Object3DFbxTangents.cpp
    src/iiActorObject/Object3DFbxAnimation.cpp
    src/iiActorObject/Object3DFbxAnimationImport.cpp
    src/iiActorObject/Object3DFbx.cpp
    src/iiActorObject/Object3DFbxImport.cpp
    src/third_party/ufbx/ufbx.cpp
    src/third_party/mikktspace/mikktspace.cpp)
find_package(iiJson 0.1 CONFIG REQUIRED)
target_link_libraries(iiActorObject PRIVATE iiJson::iiJson)
if(IIACTOROBJECT_WITH_NSCHARACTER)
    target_sources(iiActorObject PRIVATE
        src/iiActorObject/NSCharacter/CharacterAsset.cpp
        src/iiActorObject/NSCharacter/HumanoidGenerator.cpp
        src/iiActorObject/NSCharacter/CharacterEditor.cpp
        src/iiActorObject/NSCharacter/CharacterInterchange.cpp
        src/iiActorObject/NSCharacter/HumanPoseDirections.cpp
        src/iiActorObject/NSCharacter/HumanPosePlanner.cpp
        src/iiActorObject/NSCharacter/HumanAutomaticCorrectives.cpp
        src/iiActorObject/NSCharacter/HumanPoseRetarget.cpp
        src/iiActorObject/NSCharacter/HumanSkin.cpp
        src/iiActorObject/NSCharacter/HumanJointNaming.cpp
        src/iiActorObject/NSCharacter/RigSamples.cpp
        src/iiActorObject/NSCharacter/HumanoidCharacterization.cpp)
    target_sources(iiActorObject PRIVATE src/iiActorObject/NSCharacter/CharacterTemplate.cpp)
    target_sources(iiActorObject PRIVATE src/iiActorObject/NSCharacter/HumanMorphology.cpp)
    target_sources(iiActorObject PRIVATE src/iiActorObject/NSCharacter/DnaCharacter.cpp)
endif()
if(MSVC)
    set_source_files_properties(src/third_party/ufbx/ufbx.cpp src/third_party/mikktspace/mikktspace.cpp PROPERTIES COMPILE_OPTIONS /w)
else()
    set_source_files_properties(src/third_party/ufbx/ufbx.cpp src/third_party/mikktspace/mikktspace.cpp PROPERTIES COMPILE_OPTIONS -w)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_source_files_properties(src/iiActorObject/Object3DFbxImport.cpp PROPERTIES COMPILE_OPTIONS "-Wno-gnu-anonymous-struct;-Wno-nested-anon-types")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_source_files_properties(src/iiActorObject/Object3DFbxImport.cpp PROPERTIES COMPILE_OPTIONS -Wno-pedantic)
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
    add_library(iiActorObjectVisualCodec STATIC src/iiActorObject/FbxVisualImageResolver.cpp)
    add_library(iiActorObject::VisualCodec ALIAS iiActorObjectVisualCodec)
    target_compile_features(iiActorObjectVisualCodec PUBLIC cxx_std_23)
    target_link_libraries(iiActorObjectVisualCodec PUBLIC iiActorObject::iiActorObject PRIVATE iiVisualCodec::Image)
    set_target_properties(iiActorObjectVisualCodec PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON EXPORT_NAME VisualCodec)
    install(TARGETS iiActorObjectVisualCodec EXPORT iiActorObjectVisualCodecTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES src/iiActorObject/FbxVisualImageResolver.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject)
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
    add_executable(iiActorObjectProgressiveMorphRigTests tests/ProgressiveMorphRigTest.cpp src/third_party/ufbx/ufbx.cpp)
    target_link_libraries(iiActorObjectProgressiveMorphRigTests PRIVATE iiActorObject::iiActorObject)
    target_compile_features(iiActorObjectProgressiveMorphRigTests PRIVATE cxx_std_23)
    set_target_properties(iiActorObjectProgressiveMorphRigTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case curve invalid project lifetime split)
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
    add_executable(iiActorObjectPortableNumberTests tests/PortableNumberParsingTest.cpp)
    target_compile_features(iiActorObjectPortableNumberTests PRIVATE cxx_std_23)
    target_include_directories(iiActorObjectPortableNumberTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
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
    target_link_libraries(iiActorObjectInstanceTransferTests PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObjectInstanceTransferTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case animated world safeguards deformation_bind scene topology synchronized)
        add_test(NAME iiActorObject.instance_transfer.${_case} COMMAND iiActorObjectInstanceTransferTests ${_case})
    endforeach()
    add_executable(iiActorObject3DEngineTests tests/Object3DEngineTest.cpp)
    target_link_libraries(iiActorObject3DEngineTests PRIVATE iiActorObject::iiActorObject)
    foreach(_case generic collection surface limits)
        add_test(NAME iiActorObject.object3d.${_case} COMMAND iiActorObject3DEngineTests ${_case})
    endforeach()
    if(IIACTOROBJECT_WITH_NSCHARACTER)
        add_executable(iiActorObjectHumanAccessoriesTests tests/HumanAccessoriesTest.cpp)
        target_link_libraries(iiActorObjectHumanAccessoriesTests PRIVATE iiActorObject::iiActorObject)
        set_target_properties(iiActorObjectHumanAccessoriesTests PROPERTIES CXX_EXTENSIONS OFF)
        foreach(_case catalog human alternate editing editing_large batch visibility removal addition composition retained_slot rig_follow instance_follow grounded failures)
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
    src/iiActorObject/GlbDocument.h
    src/iiActorObject/GlbAnimationSource.h
    src/iiActorObject/GlbAnimationPose.h
    src/iiActorObject/GlbAnimationBake.h
    src/iiActorObject/Object3DAnimation.h
    src/iiActorObject/Object3DProjectAnimation.h
    src/iiActorObject/Actor.h
    src/iiActorObject/ActorComponent.h
    src/iiActorObject/SceneComponent.h
    src/iiActorObject/Transform.h
    src/iiActorObject/Usd.h
    src/iiActorObject/Skeleton.h
    src/iiActorObject/Animation.h
    src/iiActorObject/SkeletalMesh.h
    src/iiActorObject/DualQuaternionSkinning.h
    src/iiActorObject/CharacterRig.h
    src/iiActorObject/Character.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/Export.h" "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/Features.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject)
install(FILES
    src/iiActorObject/Object3DAsset.h
    src/iiActorObject/Object3DImage.h
    src/iiActorObject/Object3DBvh.h
    src/iiActorObject/Object3DPoseRig.h
    src/iiActorObject/Object3DPoseCorrectives.h
    src/iiActorObject/Object3DAutomaticCorrectives.h
    src/iiActorObject/Object3DPoseCorrectiveAnimation.h
    src/iiActorObject/Object3DPoseMorphs.h
    src/iiActorObject/Object3DPoseSnapshot.h
    src/iiActorObject/Object3DRigSnapshot.h
    src/iiActorObject/Object3DRigSamples.h
    src/iiActorObject/RigControlSpace.h
    src/iiActorObject/RigSampleCatalog.h
    src/iiActorObject/PoseAiming.h
    src/iiActorObject/PoseFrames.h
    src/iiActorObject/PoseReference.h
    src/iiActorObject/PoseRetarget.h
    src/iiActorObject/AnimationRetarget.h
    src/iiActorObject/Object3DProxy.h
    src/iiActorObject/Object3DTemplate.h
    src/iiActorObject/Object3DTemplateMorphs.h
    src/iiActorObject/Object3DProgressiveMorph.h
    src/iiActorObject/Object3DTemplateRigTransfer.h
    src/iiActorObject/Object3DTemplateAdapt.h
    src/iiActorObject/Object3DObj.h
    src/iiActorObject/Object3DEngine.h
    src/iiActorObject/Object3DEditor.h
    src/iiActorObject/Object3DEditorRig.h
    src/iiActorObject/Object3DInstanceTransfer.h
    src/iiActorObject/Object3DRig.h
    src/iiActorObject/Object3DDna.h
    src/iiActorObject/Object3DInterchange.h
    src/iiActorObject/Object3DProject.h
    src/iiActorObject/MeshGeometry.h
    src/iiActorObject/MeshTangents.h
    src/iiActorObject/MeshTopology.h
    src/iiActorObject/MeshPrimitives.h
    src/iiActorObject/SkinBinding.h
    src/iiActorObject/MeshSurface.h
    src/iiActorObject/SurfaceQuery.h
    src/iiActorObject/SurfaceCollider.h
    src/iiActorObject/MeshCollider.h
    src/iiActorObject/SurfaceFitting.h
    src/iiActorObject/SurfaceFittingRules.h
    src/iiActorObject/SurfaceBinding.h
    src/iiActorObject/MeshEditing.h
    src/iiActorObject/MeshScreenSelection.h
    src/iiActorObject/MeshMaskProjection.h
    src/iiActorObject/MeshSymmetry.h
    src/iiActorObject/MorphSymmetry.h
    src/iiActorObject/SkinPainting.h
    src/iiActorObject/SkeletonEditing.h
    src/iiActorObject/SkeletonScale.h
    src/iiActorObject/Object3DExternalEdits.h
    src/iiActorObject/ImplicitSurface.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject)
if(IIACTOROBJECT_WITH_NSCHARACTER)
install(FILES
    src/iiActorObject/NSCharacter/NSCharacter.h
    src/iiActorObject/NSCharacter/CharacterAsset.h
    src/iiActorObject/NSCharacter/CharacterTemplate.h
    src/iiActorObject/NSCharacter/HumanMorphology.h
    src/iiActorObject/NSCharacter/HumanSkin.h
    src/iiActorObject/NSCharacter/HumanoidCharacterization.h
    src/iiActorObject/NSCharacter/HumanPoseDirections.h
    src/iiActorObject/NSCharacter/HumanPosePlanner.h
    src/iiActorObject/NSCharacter/HumanAutomaticCorrectives.h
    src/iiActorObject/NSCharacter/HumanPoseRetarget.h
    src/iiActorObject/NSCharacter/HumanJointNaming.h
    src/iiActorObject/NSCharacter/RigSamples.h
    src/iiActorObject/NSCharacter/AutoRig.h
    src/iiActorObject/NSCharacter/HumanoidGenerator.h
    src/iiActorObject/NSCharacter/CharacterEditor.h
    src/iiActorObject/NSCharacter/CharacterInterchange.h
    src/iiActorObject/NSCharacter/DnaCharacter.h
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
install(FILES example/BlenderExternalEdits.py DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/integration")
