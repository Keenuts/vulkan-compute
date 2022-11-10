set(SOURCES
    ${SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
)

if (DEFINED USE_GLSLANG)
  add_custom_command(
      OUTPUT sum.spv
      MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/sum.comp"
      COMMAND ${GLSLANG}
          -V ${CMAKE_CURRENT_SOURCE_DIR}/sum.comp
          -o ${CMAKE_BINARY_DIR}/sum.spv
          -DELT_COUNT=${ELT_COUNT}
          -DWORKGROUP_SIZE=${WORKGROUP_SIZE}
          --target-env vulkan1.2
          -Os
  )

  add_custom_target(
      shaders ALL
      DEPENDS sum.spv
  )
else ()
  add_custom_command(
      OUTPUT sum.spv
      MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/sum.hlsl"
      COMMAND ${DXC}
          ${CMAKE_CURRENT_SOURCE_DIR}/sum.hlsl
          -Fo ${CMAKE_BINARY_DIR}/sum.spv
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
endif ()

add_executable(sum ${SOURCES})
add_dependencies(sum shaders)
