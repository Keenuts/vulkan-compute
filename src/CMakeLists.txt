set(SOURCES
    ${SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
)

if ("${SHADER_LANGUAGE}" MATCHES "GLSL")
  add_custom_command(
      OUTPUT sum.spv
      MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/sum.glsl"
      COMMAND ${GLSLANG}
          -V ${CMAKE_CURRENT_SOURCE_DIR}/sum.glsl
          -o ${CMAKE_BINARY_DIR}/sum.glsl.spv
          -S comp
          -DELT_COUNT=${ELT_COUNT}
          -DWORKGROUP_SIZE=${WORKGROUP_SIZE}
          --target-env vulkan1.2
          -Os
  )

  add_custom_target(
      shaders ALL
      DEPENDS sum.spv
  )
elseif ("${SHADER_LANGUAGE}" MATCHES "HLSL")
  add_custom_command(
      OUTPUT sum.spv
      MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/sum.hlsl"
      COMMAND ${DXC}
          ${CMAKE_CURRENT_SOURCE_DIR}/sum.hlsl
          -Fo ${CMAKE_BINARY_DIR}/sum.hlsl.spv
          -DELT_COUNT=${ELT_COUNT}
          -DWORKGROUP_SIZE=${WORKGROUP_SIZE}
          -T cs_6_5
          -spirv
          -E main
          -fspv-target-env=vulkan1.2
  )

  add_custom_target(
      shaders ALL
      DEPENDS sum.spv
  )
elseif ("${SHADER_LANGUAGE}" MATCHES "WGSL")
  add_custom_command(
      OUTPUT sum.spv
      MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/sum.wgsl"
      COMMAND ${TINT}
          ${CMAKE_CURRENT_SOURCE_DIR}/sum.wgsl
          -o ${CMAKE_BINARY_DIR}/sum.wgsl.spv
          --format spirv
  )

  add_custom_target(
      shaders ALL
      DEPENDS sum.spv
  )
else ()
  message(FATAL_ERROR "set SHADER_LANGUAGE to either GLSL,HLSL or WGSL.")
endif ()

add_executable(sum ${SOURCES})
add_dependencies(sum shaders)
