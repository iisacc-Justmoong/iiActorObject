cmake_minimum_required(VERSION 3.31)
# iisdk:consumer-application
project(iiActorObjectVisualCodec VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
find_package(iiActorObject 0.2.1 CONFIG REQUIRED)
find_package(iiVisualCodec 0.2 CONFIG REQUIRED COMPONENTS Image)
add_library(iiActorObjectVisualCodec STATIC src/iiActorObject/FbxVisualImageResolver.cpp)
add_library(iiActorObject::VisualCodec ALIAS iiActorObjectVisualCodec)
target_compile_features(iiActorObjectVisualCodec PUBLIC cxx_std_23)
target_include_directories(iiActorObjectVisualCodec PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(iiActorObjectVisualCodec PUBLIC iiActorObject::iiActorObject PRIVATE iiVisualCodec::Image)
set_target_properties(iiActorObjectVisualCodec PROPERTIES EXPORT_NAME VisualCodec CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_executable(iiActorObjectFbxVisualImageTests tests/FbxVisualImageResolverTest.cpp)
    target_link_libraries(iiActorObjectFbxVisualImageTests PRIVATE iiActorObject::VisualCodec iiVisualCodec::Image)
    set_target_properties(iiActorObjectFbxVisualImageTests PROPERTIES CXX_EXTENSIONS OFF)
    set(_fixtures "${CMAKE_CURRENT_SOURCE_DIR}/../../resources/fixtures")
    foreach(_case resolution limits passthrough embedded)
        add_test(NAME iiActorObject.fbx_visual_image.${_case} COMMAND iiActorObjectFbxVisualImageTests ${_case}
            "${_fixtures}/fbx/external-textures-ascii.fbx" "${_fixtures}/images/checker.png"
            "${_fixtures}/fbx/normal-map-ascii.fbx")
    endforeach()
    add_test(NAME iiActorObject.fbx_visual_image.independent COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyResolvedImageFormats.py"
        "$<TARGET_FILE:iiActorObjectFbxVisualImageTests>" "${_fixtures}/fbx/external-textures-ascii.fbx"
        "${_fixtures}/images/checker.png" "${_fixtures}/fbx/normal-map-ascii.fbx")
    foreach(_case resolution limits passthrough embedded independent)
        if(WIN32)
            set_tests_properties(iiActorObject.fbx_visual_image.${_case} PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
        else()
            set_tests_properties(iiActorObject.fbx_visual_image.${_case} PROPERTIES
                ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=unset:;DYLD_FALLBACK_LIBRARY_PATH=unset:;DYLD_FRAMEWORK_PATH=unset:;LD_LIBRARY_PATH=unset:")
        endif()
    endforeach()
endif()
set(_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/iiActorObjectVisualCodec")
install(TARGETS iiActorObjectVisualCodec EXPORT iiActorObjectVisualCodecTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT iiActorObjectVisualCodecTargets NAMESPACE iiActorObject:: DESTINATION "${_package_dir}")
install(FILES src/iiActorObject/FbxVisualImageResolver.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject")
configure_package_config_file(cmake/iiActorObjectVisualCodecConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectVisualCodecConfig.cmake" INSTALL_DESTINATION "${_package_dir}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectVisualCodecConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectVisualCodecConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectVisualCodecConfigVersion.cmake" DESTINATION "${_package_dir}")
install(FILES README.md AGENT.md docs/Boundary.md docs/Boundary.agent.json
    DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObjectVisualCodec")
