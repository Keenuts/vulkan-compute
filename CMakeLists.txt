cmake_minimum_required(VERSION 2.8)
project("sum")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG TRUE)
endif()


set(ELT_COUNT 1024)
set(WORKGROUP_SIZE 32)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}\
    -DELT_COUNT=${ELT_COUNT}\
    -DWORKGROUP_SIZE=${WORKGROUP_SIZE}"
)

set(CMAKE_C_FLAGS_DEBUG "\
    ${CMAKE_C_FLAGS_DEBUG}\
    -Wall \
    -Wextra \
    -Wno-unused-parameter \
    -Wno-unused-function \
    -g \
    -O0 \
    -Werror"
)

set(CMAKE_C_FLAGS_RELEASE "\
    ${CMAKE_C_FLAGS_RELEASE}\
    -O0\
    -g"
)

if (DEBUG)
    message("build-type: debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}")
else()
    message("build-type: release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE}")
endif()

# dependencies

find_program(GLSLANG glslangValidator)
if (NOT GLSLANG)
    message(FATAL_ERROR "GlslangValidator is missing.")
endif()


if (DEFINED ICD_LIB)
    message("linking with a custom icd")
    link_libraries(${ICD_LIB})
else ()
    link_libraries(Vulkan)
endif()

if (DEFINED VIRGL_LIB)
    message("linking with localy built static virgl")
    link_libraries(${VIRGL_LIB})
    link_libraries(dl)
    link_libraries(drm)
    link_libraries(epoxy)
    link_libraries(gbm)
    link_libraries(m)
    link_libraries(pthread)
endif()

add_subdirectory(src)
