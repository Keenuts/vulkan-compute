cmake_minimum_required(VERSION 3.4 FATAL_ERROR)
project("sum")

option(SHADER_LANGUAGE "building GLSL or HLSL shader")

message(NOTICE "shader language: ${SHADER_LANGUAGE}")

if (CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(DEBUG TRUE)
endif ()


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
    -fdiagnostics-color=always \
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
if ("${SHADER_LANGUAGE}" MATCHES "GLSL")
  message("compiler: glslang")

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_GLSLANG")
  find_program(GLSLANG glslangValidator)
  if (NOT GLSLANG)
    message(FATAL_ERROR "GlslangValidator is missing.")
  endif()

elseif ("${SHADER_LANGUAGE}" MATCHES "HLSL")
  message("compiler: dxc")

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_DXC")
  find_program(DXC dxc)
  if (NOT DXC)
    message(FATAL_ERROR "DXC is missing.")
  endif()
else ()
  message(FATAL_ERROR "set SHADER_LANGUAGE to either GLSL or HLSL")
endif ()


link_libraries(vulkan)

add_subdirectory(src)
