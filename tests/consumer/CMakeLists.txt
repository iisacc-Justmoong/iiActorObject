cmake_minimum_required(VERSION 3.24)
project(iiActorObjectInstalledConsumer LANGUAGES CXX)

find_package(iiActorObject 0.1.0 CONFIG REQUIRED)
add_executable(iiActorObject_installed_consumer ../hello_world.cpp)
target_link_libraries(iiActorObject_installed_consumer PRIVATE iiActorObject::iiActorObject)
set_target_properties(iiActorObject_installed_consumer PROPERTIES CXX_EXTENSIONS OFF)

enable_testing()
add_test(NAME iiActorObject.installed_consumer COMMAND iiActorObject_installed_consumer)
