cmake_minimum_required(VERSION 2.8)

if (CMAKE_BUILD_TYPE MATCHES "")
    set(CMAKE_BUILD_TYPE "Release")
endif ()

project("sum")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

link_libraries(vulkan)

set(ELT_COUNT 1024)
set(WORKGROUP_SIZE 32)

set(CMAKE_C_FLAGS "\
    -DELT_COUNT=${ELT_COUNT}\
    -DWORKGROUP_SIZE=${WORKGROUP_SIZE}"
)

set(CMAKE_C_FLAGS_DEBUG "\
    ${CMAKE_C_FLAGS}\
    -Wall \
    -Wextra \
    -Wno-unused-parameter \
    -Wno-unused-function \
    -g \
    -O0 \
    -Werror\
    -DELT_COUNT=${ELT_COUNT}\
    -DWORKGROUP_SIZE=${WORKGROUP_SIZE}"
)

set(CMAKE_C_FLAGS_RELEASE "\
    ${CMAKE_C_FLAGS}\
    -O3"
)

add_subdirectory(src)
