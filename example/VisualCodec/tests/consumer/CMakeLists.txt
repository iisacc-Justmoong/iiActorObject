cmake_minimum_required(VERSION 3.31)
project(iiActorObjectVisualCodecConsumer LANGUAGES CXX)
include(CTest)
find_package(iiActorObjectVisualCodec 0.1 CONFIG REQUIRED)
add_executable(consumer consumer.cpp)
target_link_libraries(consumer PRIVATE iiActorObject::VisualCodec)
add_test(NAME iiActorObject.VisualCodec.installed_consumer COMMAND consumer)
if(WIN32)
    set_tests_properties(iiActorObject.VisualCodec.installed_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:iiActorObject::iiActorObject>")
else()
    set_tests_properties(iiActorObject.VisualCodec.installed_consumer PROPERTIES
        ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=unset:;DYLD_FALLBACK_LIBRARY_PATH=unset:;DYLD_FRAMEWORK_PATH=unset:;LD_LIBRARY_PATH=unset:")
endif()
