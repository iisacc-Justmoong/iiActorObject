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

add_library(iiActorObject iiActorObject.cpp
    src/ActorComponent.cpp src/SceneComponent.cpp src/Transform.cpp src/Usd.cpp)
add_library(iiActorObject::iiActorObject ALIAS iiActorObject)
target_compile_features(iiActorObject PUBLIC cxx_std_23)
generate_export_header(iiActorObject BASE_NAME IIACTOROBJECT
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/Export.h")
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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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

if(BUILD_TESTING)
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
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/iiActorObject/Export.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiActorObject)
install(EXPORT iiActorObjectTargets NAMESPACE iiActorObject:: DESTINATION "${_package_dir}")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectConfigVersion.cmake"
    DESTINATION "${_package_dir}"
)
install(FILES README.md DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject")
install(DIRECTORY docs/ DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject/docs")
