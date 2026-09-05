cmake_minimum_required(VERSION 3.24)
project(iiActorObject VERSION 0.1.0 LANGUAGES CXX)

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
include(CTest)
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core)

add_library(iiActorObject SHARED iiActorObject.h iiActorObject.cpp)
add_library(iiActorObject::iiActorObject ALIAS iiActorObject)
target_compile_features(iiActorObject PUBLIC cxx_std_20)
target_compile_definitions(iiActorObject PRIVATE IIACTOROBJECT_BUILDING_LIBRARY)
target_link_libraries(iiActorObject PUBLIC Qt6::Core)
target_include_directories(iiActorObject PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(iiActorObject PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

if(BUILD_TESTING)
    add_executable(iiActorObject_hello_world tests/hello_world.cpp)
    target_link_libraries(iiActorObject_hello_world PRIVATE iiActorObject::iiActorObject)
    set_target_properties(iiActorObject_hello_world PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME iiActorObject.hello_world COMMAND iiActorObject_hello_world)
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
    COMPATIBILITY SameMajorVersion
)
install(TARGETS iiActorObject EXPORT iiActorObjectTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES iiActorObject.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT iiActorObjectTargets NAMESPACE iiActorObject:: DESTINATION "${_package_dir}")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iiActorObjectConfigVersion.cmake"
    DESTINATION "${_package_dir}"
)
install(FILES README.md DESTINATION "${CMAKE_INSTALL_DATADIR}/iiActorObject")
