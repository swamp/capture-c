cmake_minimum_required(VERSION 3.17)
project(swamp_capture_lib C)

set(CMAKE_C_STANDARD 99)

set(deps ../../deps/)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else()
    set(isDebug FALSE)
endif()

if (isDebug)
    message("Debug build detected")
    add_compile_definitions(CONFIGURATION_DEBUG TYRAN_CONFIGURATION_DEBUG)
    set(CONFIGURATION_DEBUG 1)
endif()

file(GLOB_RECURSE deps_src FOLLOW_SYMLINKS
        "${deps}*/src/lib/*.c"
        )

file(GLOB_RECURSE lib_src FOLLOW_SYMLINKS
        "*.c"
        )

add_library(swamp_capture_lib STATIC
    ${deps_src}
    ${lib_src}
)


target_include_directories(swamp_capture_lib PRIVATE ${deps}runtime-c/src/include)
target_include_directories(swamp_capture_lib PRIVATE ${deps}dump-c/src/include)
target_include_directories(swamp_capture_lib PRIVATE ${deps}typeinfo-c/src/include)
target_include_directories(swamp_capture_lib PRIVATE ${deps}tiny-libc/src/include)
target_include_directories(swamp_capture_lib PRIVATE ${deps}flood-c/src/include)
target_include_directories(swamp_capture_lib PRIVATE ${deps}raff-c/src/include)
target_include_directories(swamp_capture_lib PRIVATE ${deps}clog/src/include)
target_include_directories(swamp_capture_lib PUBLIC ../include)


target_link_libraries(swamp_capture_lib m)
