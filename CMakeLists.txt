cmake_minimum_required(VERSION 2.8)

project("sum")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

link_libraries(vulkan)

set(CMAKE_C_FLAGS_DEBUG "\
    -Wall \
    -Wextra \
    -Wno-unused-parameter \
    -Wno-unused-function \
    -g \
    -O0 \
    -Werror"
)
set(CMAKE_C_FLAGS_RELEASE "-O3")

message(${CMAKE_C_FLAGS_DEBUG})
add_subdirectory(src)
