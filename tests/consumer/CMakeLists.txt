cmake_minimum_required(VERSION 3.24)
project(iiActorObjectInstalledConsumer LANGUAGES CXX)

find_package(iiActorObject 0.2.1 CONFIG REQUIRED)
if(TARGET Qt6::Core)
    message(FATAL_ERROR "iiActorObject must not import Qt")
endif()
add_executable(iiActorObject_installed_consumer main.cpp)
target_link_libraries(iiActorObject_installed_consumer PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_consumer PROPERTIES CXX_EXTENSIONS OFF)

enable_testing()
add_test(NAME iiActorObject.installed_consumer COMMAND iiActorObject_installed_consumer)
if(WIN32)
    set_tests_properties(iiActorObject.installed_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
endif()
