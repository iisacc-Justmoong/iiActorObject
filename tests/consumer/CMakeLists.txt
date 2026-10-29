cmake_minimum_required(VERSION 3.24)
project(iiActorObjectInstalledConsumer LANGUAGES CXX)

find_package(iiActorObject 0.2.1 CONFIG REQUIRED)
if(TARGET Qt6::Core)
    message(FATAL_ERROR "iiActorObject must not import Qt")
endif()

add_executable(iiActorObject_installed_consumer main.cpp)
target_link_libraries(iiActorObject_installed_consumer PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_consumer PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObject_installed_character_headers
    ../CharacterTemplateCoreHeaderTest.cpp ../MakeHumanTemplateHeaderTest.cpp ../HumanAccessoriesHeaderTest.cpp)
target_link_libraries(iiActorObject_installed_character_headers PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_character_headers PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObject_installed_progressive_headers
    ../ProgressiveMorphDataHeaderTest.cpp ../ProgressiveMorphCoreHeaderTest.cpp ../ProgressiveMorphInterchangeHeaderTest.cpp
    ../ProgressiveMorphCompatibilityHeaderTest.cpp)
target_link_libraries(iiActorObject_installed_progressive_headers PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_progressive_headers PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObject_installed_glb_animation_options ../GlbAnimationOptionsHeaderTest.cpp)
target_link_libraries(iiActorObject_installed_glb_animation_options PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_glb_animation_options PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObject_installed_glb_import_options ../GlbImportOptionsHeaderTest.cpp ../GlbImageResolverHeaderTest.cpp)
target_link_libraries(iiActorObject_installed_glb_import_options PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_glb_import_options PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObject_installed_mesh_headers
    ../BoneIndexHeaderTest.cpp ../MeshDataHeaderTest.cpp ../MeshTopologyCoreHeaderTest.cpp
    ../MeshGeometryCoreHeaderTest.cpp ../MeshCompatibilityHeaderTest.cpp
    ../SurfaceBindingCoreHeaderTest.cpp ../SurfaceAttachmentHeaderTest.cpp
    ../SurfaceCompatibilityHeaderTest.cpp ../Object3DDataHeaderTest.cpp ../Object3DMaterialHeaderTest.cpp ../Object3DMeshHeaderTest.cpp
    ../Object3DAssetCompatibilityHeaderTest.cpp ../MeshSelectionCoreHeaderTest.cpp
    ../MeshSelectionAssetHeaderTest.cpp ../MeshSelectionCompatibilityHeaderTest.cpp
    ../SkeletalMeshCoreHeaderTest.cpp ../SkeletalMeshComponentHeaderTest.cpp)
target_link_libraries(iiActorObject_installed_mesh_headers PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_mesh_headers PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObject_installed_instance_transfer_headers
    ../InstanceTransferCoreHeaderTest.cpp ../InstanceTransferEditorHeaderTest.cpp
    ../InstanceTransferCompatibilityHeaderTest.cpp)
target_link_libraries(iiActorObject_installed_instance_transfer_headers PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_instance_transfer_headers PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObject_installed_rig_sample_catalog_headers
    ../RigSampleCatalogCoreHeaderTest.cpp ../RigSampleCatalogExportHeaderTest.cpp
    ../RigSampleCatalogCompatibilityHeaderTest.cpp)
target_link_libraries(iiActorObject_installed_rig_sample_catalog_headers PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_rig_sample_catalog_headers PROPERTIES CXX_EXTENSIONS OFF)

add_executable(iiActorObject_installed_fbx_resolver_header ../FbxResolverHeaderTest.cpp)
target_link_libraries(iiActorObject_installed_fbx_resolver_header PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_fbx_resolver_header PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObject_installed_tangent_consumer ../TangentInstalledConsumer.cpp)
target_link_libraries(iiActorObject_installed_tangent_consumer PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_tangent_consumer PROPERTIES CXX_EXTENSIONS OFF)
add_executable(iiActorObject_installed_project_data_headers ../ProjectDataHeadersTest.cpp
        ../ProjectDataMeshPointMaskHeaderTest.cpp ../ProjectDataMeshPointMaskValidationHeaderTest.cpp ../ProjectDataObject3DEditLayerDataHeaderTest.cpp ../ProjectDataObject3DPoseProgramDataHeaderTest.cpp ../ProjectDataObject3DProjectOptionsHeaderTest.cpp ../ProjectDataObject3DTemplateMorphDataHeaderTest.cpp ../ProjectDataObject3DTemplateTypesHeaderTest.cpp ../ProjectDataSurfaceFitSettingsHeaderTest.cpp ../ExternalIdentityHeaderTest.cpp)
target_link_libraries(iiActorObject_installed_project_data_headers PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_project_data_headers PROPERTIES CXX_EXTENSIONS OFF)
enable_testing()
add_test(NAME iiActorObject.installed_project_data_headers COMMAND iiActorObject_installed_project_data_headers)
add_executable(iiActorObject_installed_mask_validation_consumer ../PointMaskValidationInstalledConsumer.cpp)
target_link_libraries(iiActorObject_installed_mask_validation_consumer PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_mask_validation_consumer PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME iiActorObject.installed_mask_validation_consumer COMMAND iiActorObject_installed_mask_validation_consumer)
add_test(NAME iiActorObject.installed_fbx_resolver_header COMMAND iiActorObject_installed_fbx_resolver_header)
add_test(NAME iiActorObject.installed_tangent_consumer COMMAND iiActorObject_installed_tangent_consumer)
add_test(NAME iiActorObject.installed_character_headers COMMAND iiActorObject_installed_character_headers)
add_test(NAME iiActorObject.installed_progressive_headers COMMAND iiActorObject_installed_progressive_headers)
add_test(NAME iiActorObject.installed_glb_animation_options COMMAND iiActorObject_installed_glb_animation_options)
add_test(NAME iiActorObject.installed_glb_import_options COMMAND iiActorObject_installed_glb_import_options)
add_test(NAME iiActorObject.installed_mesh_headers COMMAND iiActorObject_installed_mesh_headers)
add_test(NAME iiActorObject.installed_instance_transfer_headers COMMAND iiActorObject_installed_instance_transfer_headers)
add_test(NAME iiActorObject.installed_rig_sample_catalog_headers COMMAND iiActorObject_installed_rig_sample_catalog_headers)
if(WIN32)
    set_tests_properties(iiActorObject.installed_tangent_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_fbx_resolver_header PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_mask_validation_consumer iiActorObject.installed_project_data_headers iiActorObject.installed_character_headers iiActorObject.installed_progressive_headers iiActorObject.installed_glb_animation_options iiActorObject.installed_glb_import_options iiActorObject.installed_mesh_headers iiActorObject.installed_instance_transfer_headers iiActorObject.installed_rig_sample_catalog_headers PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()
add_test(NAME iiActorObject.installed_editor_rig_consumer COMMAND iiActorObject_installed_consumer --editor-rig)
add_test(NAME iiActorObject.installed_instance_transfer_consumer COMMAND iiActorObject_installed_consumer --instance-transfer)
if(WIN32)
    set_tests_properties(iiActorObject.installed_editor_rig_consumer iiActorObject.installed_instance_transfer_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()
add_test(NAME iiActorObject.installed_consumer COMMAND iiActorObject_installed_consumer)
add_test(NAME iiActorObject.installed_proxy_coverage_consumer COMMAND iiActorObject_installed_consumer --proxy-coverage)
add_test(NAME iiActorObject.installed_template_surface_swap_consumer COMMAND iiActorObject_installed_consumer --template-surface-swap)
if(WIN32)
    set_tests_properties(iiActorObject.installed_proxy_coverage_consumer iiActorObject.installed_template_surface_swap_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()
set(IIACTOROBJECT_HUMAN_DATA_DIRECTORY "" CACHE PATH "Optional installed CC0 human template for consumer tests")
set(IIACTOROBJECT_ACCESSORY_DATA_DIRECTORY "" CACHE PATH "Optional installed CC0 accessory catalogue for consumer tests")
if(IIACTOROBJECT_HUMAN_DATA_DIRECTORY AND IIACTOROBJECT_ACCESSORY_DATA_DIRECTORY)
    add_test(NAME iiActorObject.installed_human_accessories_consumer COMMAND iiActorObject_installed_consumer
        --human-accessories "${IIACTOROBJECT_HUMAN_DATA_DIRECTORY}" "${IIACTOROBJECT_ACCESSORY_DATA_DIRECTORY}")
    if(WIN32)
        set_tests_properties(iiActorObject.installed_human_accessories_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()
add_test(NAME iiActorObject.installed_pose_corrective_consumer COMMAND iiActorObject_installed_consumer --pose-corrective)
add_test(NAME iiActorObject.installed_automatic_corrective_consumer COMMAND iiActorObject_installed_consumer --automatic-corrective)
add_test(NAME iiActorObject.installed_screen_selection_consumer COMMAND iiActorObject_installed_consumer --screen-selection)
add_test(NAME iiActorObject.installed_selection_transfer_consumer COMMAND iiActorObject_installed_consumer --selection-transfer)
add_test(NAME iiActorObject.installed_rig_sample_skinning_consumer COMMAND iiActorObject_installed_consumer --rig-sample-skinning)
add_test(NAME iiActorObject.installed_glb_mixed_consumer COMMAND iiActorObject_installed_consumer --glb-mixed)
add_test(NAME iiActorObject.installed_material_surface_consumer COMMAND iiActorObject_installed_consumer --material-surface)
add_test(NAME iiActorObject.installed_group_pose_consumer COMMAND iiActorObject_installed_consumer --group-pose)
add_test(NAME iiActorObject.installed_progressive_morph_consumer COMMAND iiActorObject_installed_consumer --progressive-morph
    "${CMAKE_CURRENT_LIST_DIR}/../../resources/fixtures/fbx/progressive-morph-ascii.fbx"
    "${CMAKE_CURRENT_LIST_DIR}/../../resources/fixtures/fbx/progressive-animated-ascii.fbx")
add_test(NAME iiActorObject.installed_template_adapt_identity_consumer COMMAND iiActorObject_installed_consumer --template-adapt-identity)
add_test(NAME iiActorObject.installed_glb_animation_consumer COMMAND iiActorObject_installed_consumer --glb-animation)
set(IIACTOROBJECT_SKIN_DATA_DIRECTORY "" CACHE PATH "Optional CC0 skin catalogue for installed consumer tests")
if(IIACTOROBJECT_SKIN_DATA_DIRECTORY)
    add_test(NAME iiActorObject.installed_human_skin_consumer COMMAND iiActorObject_installed_consumer
        --human-skin "${IIACTOROBJECT_SKIN_DATA_DIRECTORY}")
    if(WIN32)
        set_tests_properties(iiActorObject.installed_human_skin_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()
add_test(NAME iiActorObject.installed_glb_external_identity_consumer COMMAND iiActorObject_installed_consumer --glb-external-identity)
add_test(NAME iiActorObject.installed_pose_snapshot_consumer COMMAND iiActorObject_installed_consumer --pose-snapshot)
add_test(NAME iiActorObject.installed_multi_uv_consumer COMMAND iiActorObject_installed_consumer --multi-uv)
add_test(NAME iiActorObject.installed_named_selection_consumer COMMAND iiActorObject_installed_consumer --named-selection)
add_test(NAME iiActorObject.installed_grouped_morph_consumer COMMAND iiActorObject_installed_consumer --grouped-morph)
add_test(NAME iiActorObject.installed_base_normal_consumer COMMAND iiActorObject_installed_consumer --base-normal)
add_test(NAME iiActorObject.installed_fbx_animation_consumer COMMAND iiActorObject_installed_consumer --fbx-animation)
add_test(NAME iiActorObject.installed_template_rig_yaw_consumer COMMAND iiActorObject_installed_consumer --template-rig-yaw)
add_test(NAME iiActorObject.installed_mask_projection_consumer COMMAND iiActorObject_installed_consumer --mask-projection)
add_test(NAME iiActorObject.installed_dual_quaternion_consumer COMMAND iiActorObject_installed_consumer --dual-quaternion)
set(IIACTOROBJECT_NORMAL_MAP_FIXTURE "" CACHE FILEPATH "PNG fixture for the staged normal-map consumer")
if(IIACTOROBJECT_NORMAL_MAP_FIXTURE)
    add_test(NAME iiActorObject.installed_normal_map_consumer COMMAND iiActorObject_installed_consumer --normal-map "${IIACTOROBJECT_NORMAL_MAP_FIXTURE}")
    if(WIN32)
        set_tests_properties(iiActorObject.installed_normal_map_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()
if(WIN32)
    set_tests_properties(iiActorObject.installed_progressive_morph_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_group_pose_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_material_surface_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_automatic_corrective_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_screen_selection_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_selection_transfer_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_rig_sample_skinning_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_glb_mixed_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_glb_animation_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_pose_corrective_consumer iiActorObject.installed_glb_external_identity_consumer iiActorObject.installed_template_rig_yaw_consumer iiActorObject.installed_template_adapt_identity_consumer iiActorObject.installed_mask_projection_consumer iiActorObject.installed_dual_quaternion_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    set_tests_properties(iiActorObject.installed_pose_snapshot_consumer iiActorObject.installed_multi_uv_consumer iiActorObject.installed_named_selection_consumer iiActorObject.installed_grouped_morph_consumer iiActorObject.installed_base_normal_consumer iiActorObject.installed_fbx_animation_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()
add_test(NAME iiActorObject.installed_characterization_consumer COMMAND iiActorObject_installed_consumer --characterization)
if(WIN32)
    set_tests_properties(iiActorObject.installed_characterization_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()
set(IIACTOROBJECT_HUMAN_DATA_DIRECTORY "" CACHE PATH "Optional installed CC0 template data directory")
if(IIACTOROBJECT_HUMAN_DATA_DIRECTORY)
    add_test(NAME iiActorObject.installed_human_consumer COMMAND iiActorObject_installed_consumer
        --human "${IIACTOROBJECT_HUMAN_DATA_DIRECTORY}")
endif()
set(IIACTOROBJECT_DNA_TEST_FIXTURE "" CACHE FILEPATH "Optional independent DNA fixture for installed-package verification")
if(IIACTOROBJECT_DNA_TEST_FIXTURE)
    add_test(NAME iiActorObject.installed_dna_consumer COMMAND iiActorObject_installed_consumer "${IIACTOROBJECT_DNA_TEST_FIXTURE}")
    if(WIN32)
        set_tests_properties(iiActorObject.installed_dna_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()
if(WIN32)
    if(IIACTOROBJECT_HUMAN_DATA_DIRECTORY)
        set_tests_properties(iiActorObject.installed_human_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
    set_tests_properties(iiActorObject.installed_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()

set(IIACTOROBJECT_FACE_DATA_DIRECTORY "" CACHE PATH "Optional installed CC0 face data directory")
if(IIACTOROBJECT_HUMAN_DATA_DIRECTORY AND IIACTOROBJECT_FACE_DATA_DIRECTORY)
    add_test(NAME iiActorObject.installed_face_consumer COMMAND iiActorObject_installed_consumer
        --face "${IIACTOROBJECT_HUMAN_DATA_DIRECTORY}" "${IIACTOROBJECT_FACE_DATA_DIRECTORY}")
    if(WIN32)
        set_tests_properties(iiActorObject.installed_face_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()

set(IIACTOROBJECT_MACRO_DATA_DIRECTORY "" CACHE PATH "Optional installed CC0 full morphology directory")
if(IIACTOROBJECT_HUMAN_DATA_DIRECTORY AND IIACTOROBJECT_MACRO_DATA_DIRECTORY AND IIACTOROBJECT_FACE_DATA_DIRECTORY)
    add_test(NAME iiActorObject.installed_macro_consumer COMMAND iiActorObject_installed_consumer
        --macro "${IIACTOROBJECT_HUMAN_DATA_DIRECTORY}" "${IIACTOROBJECT_MACRO_DATA_DIRECTORY}" "${IIACTOROBJECT_FACE_DATA_DIRECTORY}")
    if(WIN32)
        set_tests_properties(iiActorObject.installed_macro_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()

set(IIACTOROBJECT_EXPRESSION_DATA_DIRECTORY "" CACHE PATH "Optional installed CC0 face pose directory")
if(IIACTOROBJECT_HUMAN_DATA_DIRECTORY AND IIACTOROBJECT_MACRO_DATA_DIRECTORY AND IIACTOROBJECT_FACE_DATA_DIRECTORY AND IIACTOROBJECT_EXPRESSION_DATA_DIRECTORY)
    add_test(NAME iiActorObject.installed_face_pose_consumer COMMAND iiActorObject_installed_consumer
        --pose "${IIACTOROBJECT_HUMAN_DATA_DIRECTORY}" "${IIACTOROBJECT_MACRO_DATA_DIRECTORY}"
        "${IIACTOROBJECT_FACE_DATA_DIRECTORY}" "${IIACTOROBJECT_EXPRESSION_DATA_DIRECTORY}")
    if(WIN32)
        set_tests_properties(iiActorObject.installed_face_pose_consumer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
    endif()
endif()
