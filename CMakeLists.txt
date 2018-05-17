cmake_minimum_required(VERSION 2.8)

project("sum")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

link_libraries(vulkan)


if (DEBUG)
    message("DEBUG MODE")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra -Wno-unused-parameters -g")
endif (DEBUG)

add_subdirectory(src)
