cmake_minimum_required(VERSION 3.17)
project(swamp_capture C)

set(CMAKE_C_STANDARD 11)


set(deps ../../deps/)


file(GLOB_RECURSE deps_src FOLLOW_SYMLINKS
        "${deps}*/*/src/lib/*.c"
        "${deps}*/*/src/platform/posix/*.c"
        )


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else()
    set(isDebug FALSE)
endif()

add_executable(swamp_capture_example
    ${deps_src}
    main.c
)


target_include_directories(swamp_capture_example PUBLIC ${deps}piot/monotonic-time-c/src/include)
target_include_directories(swamp_capture_example PUBLIC ${deps}piot/tinge-c/src/include)
target_include_directories(swamp_capture_example PUBLIC ${deps}piot/swamp-script-state/src/include)
target_link_libraries(swamp_capture_example swamp_capture m)
