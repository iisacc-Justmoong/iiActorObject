cmake_minimum_required(VERSION 3.24)
project(iiActorObjectCoreOnlyConsumer LANGUAGES CXX)
find_package(iiActorObject 0.2.1 CONFIG REQUIRED)
add_executable(iiActorObjectCoreOnlyConsumer main.cpp)
target_link_libraries(iiActorObjectCoreOnlyConsumer PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyConsumer PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObjectCoreOnlyProgressiveHeaders
    ../ProgressiveMorphDataHeaderTest.cpp ../ProgressiveMorphCoreHeaderTest.cpp ../ProgressiveMorphInterchangeHeaderTest.cpp
    ../ProgressiveMorphCompatibilityHeaderTest.cpp)
target_link_libraries(iiActorObjectCoreOnlyProgressiveHeaders PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyProgressiveHeaders PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObjectCoreOnlyGlbAnimationOptions ../GlbAnimationOptionsHeaderTest.cpp)
target_link_libraries(iiActorObjectCoreOnlyGlbAnimationOptions PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyGlbAnimationOptions PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObjectCoreOnlyGlbImportOptions ../GlbImportOptionsHeaderTest.cpp ../GlbImageResolverHeaderTest.cpp)
target_link_libraries(iiActorObjectCoreOnlyGlbImportOptions PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyGlbImportOptions PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObjectCoreOnlyMeshHeaders
    ../BoneIndexHeaderTest.cpp ../MeshDataHeaderTest.cpp ../MeshTopologyCoreHeaderTest.cpp
    ../MeshGeometryCoreHeaderTest.cpp ../MeshCompatibilityHeaderTest.cpp
    ../SurfaceBindingCoreHeaderTest.cpp ../SurfaceAttachmentHeaderTest.cpp
    ../SurfaceCompatibilityHeaderTest.cpp ../Object3DDataHeaderTest.cpp ../Object3DMaterialHeaderTest.cpp ../Object3DMeshHeaderTest.cpp
    ../Object3DAssetCompatibilityHeaderTest.cpp ../MeshSelectionCoreHeaderTest.cpp
    ../MeshSelectionAssetHeaderTest.cpp ../MeshSelectionCompatibilityHeaderTest.cpp
    ../SkeletalMeshCoreHeaderTest.cpp ../SkeletalMeshComponentHeaderTest.cpp)
target_link_libraries(iiActorObjectCoreOnlyMeshHeaders PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyMeshHeaders PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObjectCoreOnlyInstanceTransferHeaders
    ../InstanceTransferCoreHeaderTest.cpp ../InstanceTransferEditorHeaderTest.cpp
    ../InstanceTransferCompatibilityHeaderTest.cpp)
target_link_libraries(iiActorObjectCoreOnlyInstanceTransferHeaders PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyInstanceTransferHeaders PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObjectCoreOnlyRigSampleCatalogHeaders
    ../RigSampleCatalogCoreHeaderTest.cpp ../RigSampleCatalogExportHeaderTest.cpp
    ../RigSampleCatalogCompatibilityHeaderTest.cpp)
target_link_libraries(iiActorObjectCoreOnlyRigSampleCatalogHeaders PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyRigSampleCatalogHeaders PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObjectCoreOnlyFbxResolverHeader ../FbxResolverHeaderTest.cpp)
target_link_libraries(iiActorObjectCoreOnlyFbxResolverHeader PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyFbxResolverHeader PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObjectCoreOnlyProjectDataHeaders ../ProjectDataHeadersTest.cpp
        ../ProjectDataMeshPointMaskHeaderTest.cpp ../ProjectDataMeshPointMaskValidationHeaderTest.cpp ../ProjectDataObject3DEditLayerDataHeaderTest.cpp ../ProjectDataObject3DPoseProgramDataHeaderTest.cpp ../ProjectDataObject3DProjectOptionsHeaderTest.cpp ../ProjectDataObject3DTemplateMorphDataHeaderTest.cpp ../ProjectDataObject3DTemplateTypesHeaderTest.cpp ../ProjectDataSurfaceFitSettingsHeaderTest.cpp ../ExternalIdentityHeaderTest.cpp)
target_link_libraries(iiActorObjectCoreOnlyProjectDataHeaders PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyProjectDataHeaders PROPERTIES CXX_EXTENSIONS OFF)
enable_testing()
add_test(NAME iiActorObject.core_only_installed_project_data_headers COMMAND iiActorObjectCoreOnlyProjectDataHeaders)
add_executable(iiActorObjectCoreOnlyMaskValidationConsumer ../PointMaskValidationInstalledConsumer.cpp)
target_link_libraries(iiActorObjectCoreOnlyMaskValidationConsumer PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyMaskValidationConsumer PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME iiActorObject.core_only_installed_mask_validation_consumer COMMAND iiActorObjectCoreOnlyMaskValidationConsumer)
add_executable(iiActorObjectCoreOnlyTangentConsumer ../TangentInstalledConsumer.cpp)
target_link_libraries(iiActorObjectCoreOnlyTangentConsumer PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObjectCoreOnlyTangentConsumer PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME iiActorObject.core_only_installed_tangent_consumer COMMAND iiActorObjectCoreOnlyTangentConsumer)
add_test(NAME iiActorObject.core_only_installed_fbx_resolver_header COMMAND iiActorObjectCoreOnlyFbxResolverHeader)
add_test(NAME iiActorObject.core_only_installed_progressive_headers COMMAND iiActorObjectCoreOnlyProgressiveHeaders)
add_test(NAME iiActorObject.core_only_installed_glb_animation_options COMMAND iiActorObjectCoreOnlyGlbAnimationOptions)
add_test(NAME iiActorObject.core_only_installed_glb_import_options COMMAND iiActorObjectCoreOnlyGlbImportOptions)
add_test(NAME iiActorObject.core_only_installed_mesh_headers COMMAND iiActorObjectCoreOnlyMeshHeaders)
add_test(NAME iiActorObject.core_only_installed_instance_transfer_headers COMMAND iiActorObjectCoreOnlyInstanceTransferHeaders)
add_test(NAME iiActorObject.core_only_installed_rig_sample_catalog_headers COMMAND iiActorObjectCoreOnlyRigSampleCatalogHeaders)
add_test(NAME iiActorObject.core_only_installed_editor_rig_consumer COMMAND iiActorObjectCoreOnlyConsumer --editor-rig)
add_test(NAME iiActorObject.core_only_installed_instance_transfer_consumer COMMAND iiActorObjectCoreOnlyConsumer --instance-transfer)
if(WIN32)
    set_tests_properties(iiActorObject.core_only_installed_tangent_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_fbx_resolver_header PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_mask_validation_consumer iiActorObject.core_only_installed_project_data_headers iiActorObject.core_only_installed_progressive_headers iiActorObject.core_only_installed_glb_animation_options iiActorObject.core_only_installed_glb_import_options iiActorObject.core_only_installed_mesh_headers iiActorObject.core_only_installed_instance_transfer_headers iiActorObject.core_only_installed_rig_sample_catalog_headers iiActorObject.core_only_installed_editor_rig_consumer iiActorObject.core_only_installed_instance_transfer_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()
add_test(NAME iiActorObject.core_only_installed_consumer COMMAND iiActorObjectCoreOnlyConsumer)
add_test(NAME iiActorObject.core_only_installed_proxy_coverage_consumer COMMAND iiActorObjectCoreOnlyConsumer --proxy-coverage)
add_test(NAME iiActorObject.core_only_installed_template_surface_swap_consumer COMMAND iiActorObjectCoreOnlyConsumer --template-surface-swap)
if(WIN32)
    set_tests_properties(iiActorObject.core_only_installed_proxy_coverage_consumer iiActorObject.core_only_installed_template_surface_swap_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()
add_test(NAME iiActorObject.core_only_installed_pose_corrective_consumer COMMAND iiActorObjectCoreOnlyConsumer --pose-corrective)
add_test(NAME iiActorObject.core_only_installed_automatic_corrective_consumer COMMAND iiActorObjectCoreOnlyConsumer --automatic-corrective)
add_test(NAME iiActorObject.core_only_installed_screen_selection_consumer COMMAND iiActorObjectCoreOnlyConsumer --screen-selection)
add_test(NAME iiActorObject.core_only_installed_selection_transfer_consumer COMMAND iiActorObjectCoreOnlyConsumer --selection-transfer)
add_test(NAME iiActorObject.core_only_installed_rig_sample_skinning_consumer COMMAND iiActorObjectCoreOnlyConsumer --rig-sample-skinning)
add_test(NAME iiActorObject.core_only_installed_glb_mixed_consumer COMMAND iiActorObjectCoreOnlyConsumer --glb-mixed)
add_test(NAME iiActorObject.core_only_installed_material_surface_consumer COMMAND iiActorObjectCoreOnlyConsumer --material-surface)
add_test(NAME iiActorObject.core_only_installed_group_pose_consumer COMMAND iiActorObjectCoreOnlyConsumer --group-pose)
add_test(NAME iiActorObject.core_only_installed_progressive_morph_consumer COMMAND iiActorObjectCoreOnlyConsumer --progressive-morph
    "${CMAKE_CURRENT_LIST_DIR}/../../resources/fixtures/fbx/progressive-morph-ascii.fbx"
    "${CMAKE_CURRENT_LIST_DIR}/../../resources/fixtures/fbx/progressive-animated-ascii.fbx")
add_test(NAME iiActorObject.core_only_installed_template_adapt_identity_consumer COMMAND iiActorObjectCoreOnlyConsumer --template-adapt-identity)
add_test(NAME iiActorObject.core_only_installed_glb_animation_consumer COMMAND iiActorObjectCoreOnlyConsumer --glb-animation)
add_test(NAME iiActorObject.core_only_installed_glb_external_identity_consumer COMMAND iiActorObjectCoreOnlyConsumer --glb-external-identity)
add_test(NAME iiActorObject.core_only_installed_pose_snapshot_consumer COMMAND iiActorObjectCoreOnlyConsumer --pose-snapshot)
add_test(NAME iiActorObject.core_only_installed_multi_uv_consumer COMMAND iiActorObjectCoreOnlyConsumer --multi-uv)
add_test(NAME iiActorObject.core_only_installed_named_selection_consumer COMMAND iiActorObjectCoreOnlyConsumer --named-selection)
add_test(NAME iiActorObject.core_only_installed_grouped_morph_consumer COMMAND iiActorObjectCoreOnlyConsumer --grouped-morph)
add_test(NAME iiActorObject.core_only_installed_base_normal_consumer COMMAND iiActorObjectCoreOnlyConsumer --base-normal)
add_test(NAME iiActorObject.core_only_installed_fbx_animation_consumer COMMAND iiActorObjectCoreOnlyConsumer --fbx-animation)
add_test(NAME iiActorObject.core_only_installed_template_rig_yaw_consumer COMMAND iiActorObjectCoreOnlyConsumer --template-rig-yaw)
add_test(NAME iiActorObject.core_only_installed_mask_projection_consumer COMMAND iiActorObjectCoreOnlyConsumer --mask-projection)
add_test(NAME iiActorObject.core_only_installed_dual_quaternion_consumer COMMAND iiActorObjectCoreOnlyConsumer --dual-quaternion)
set(IIACTOROBJECT_NORMAL_MAP_FIXTURE "" CACHE FILEPATH "PNG fixture for the staged normal-map consumer")
if(IIACTOROBJECT_NORMAL_MAP_FIXTURE)
    add_test(NAME iiActorObject.core_only_installed_normal_map_consumer COMMAND iiActorObjectCoreOnlyConsumer --normal-map "${IIACTOROBJECT_NORMAL_MAP_FIXTURE}")
    if(WIN32)
        set_tests_properties(iiActorObject.core_only_installed_normal_map_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()
if(WIN32)
    set_tests_properties(iiActorObject.core_only_installed_progressive_morph_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_group_pose_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_material_surface_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_automatic_corrective_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_screen_selection_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_selection_transfer_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_rig_sample_skinning_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_glb_mixed_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_glb_animation_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_pose_corrective_consumer iiActorObject.core_only_installed_glb_external_identity_consumer iiActorObject.core_only_installed_template_rig_yaw_consumer iiActorObject.core_only_installed_template_adapt_identity_consumer iiActorObject.core_only_installed_mask_projection_consumer iiActorObject.core_only_installed_dual_quaternion_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.core_only_installed_pose_snapshot_consumer iiActorObject.core_only_installed_multi_uv_consumer iiActorObject.core_only_installed_named_selection_consumer iiActorObject.core_only_installed_grouped_morph_consumer iiActorObject.core_only_installed_base_normal_consumer iiActorObject.core_only_installed_fbx_animation_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()
set(IIACTOROBJECT_DNA_TEST_FIXTURE "" CACHE FILEPATH "Optional independent DNA fixture for installed-package verification")
if(IIACTOROBJECT_DNA_TEST_FIXTURE)
    add_test(NAME iiActorObject.core_only_installed_dna_consumer COMMAND iiActorObjectCoreOnlyConsumer "${IIACTOROBJECT_DNA_TEST_FIXTURE}")
endif()
if(WIN32)
    set_tests_properties(iiActorObject.core_only_installed_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    if(IIACTOROBJECT_DNA_TEST_FIXTURE)
        set_tests_properties(iiActorObject.core_only_installed_dna_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()

set(IIACTOROBJECT_IMAGE_TEST_FIXTURE "" CACHE FILEPATH "Optional PNG fixture for installed core resources")
if(IIACTOROBJECT_IMAGE_TEST_FIXTURE)
    add_test(NAME iiActorObject.core_only_installed_image_consumer COMMAND iiActorObjectCoreOnlyConsumer
        --image "${IIACTOROBJECT_IMAGE_TEST_FIXTURE}")
    if(WIN32)
        set_tests_properties(iiActorObject.core_only_installed_image_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()
