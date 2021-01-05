cmake_minimum_required(VERSION 3.17)
project(swamp_capture C)

set(CMAKE_C_STANDARD 99)

set(deps ../../deps/)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else()
    set(isDebug FALSE)
endif()

add_executable(swamp_capture_example
    main.c
)

target_include_directories(swamp_capture_example PRIVATE ${deps}clog/src/include)
target_include_directories(swamp_capture_example PRIVATE ${deps}tiny-libc/src/include)
target_include_directories(swamp_capture_example PRIVATE ${deps}runtime-c/src/include)
target_include_directories(swamp_capture_example PRIVATE ${deps}dump-c/src/include)
target_include_directories(swamp_capture_example PRIVATE ${deps}typeinfo-c/src/include)
target_include_directories(swamp_capture_example PRIVATE ${deps}flood-c/src/include)

target_link_libraries(swamp_capture_example swamp_capture m)
