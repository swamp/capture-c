cmake_minimum_required(VERSION 3.17)
project(swamp_capture C)

set(CMAKE_C_STANDARD 11)

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

file(GLOB_RECURSE lib_src FOLLOW_SYMLINKS
        "*.c"
        )

add_library(swamp_capture STATIC
    ${lib_src}
)


target_include_directories(swamp_capture PUBLIC ${deps}swamp/runtime-c/src/include)
target_include_directories(swamp_capture PUBLIC ${deps}swamp/dump-c/src/include)
target_include_directories(swamp_capture PUBLIC ${deps}swamp/typeinfo-c/src/include)
target_include_directories(swamp_capture PUBLIC ${deps}swamp/typeinfo-serialize-c/src/include)
target_include_directories(swamp_capture PUBLIC ${deps}swamp/snapshot-c/src/include)
target_include_directories(swamp_capture PUBLIC ${deps}piot/tiny-libc/src/include)
target_include_directories(swamp_capture PUBLIC ${deps}piot/flood-c/src/include)
target_include_directories(swamp_capture PUBLIC ${deps}piot/raff-c/src/include)
target_include_directories(swamp_capture PUBLIC ${deps}piot/clog/src/include)
target_include_directories(swamp_capture PUBLIC ../include)


target_link_libraries(swamp_capture m)
