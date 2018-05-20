set(SOURCES
    ${SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
)

add_custom_command(
    OUTPUT sum.spv
    COMMAND glslangValidator
        -V ${CMAKE_CURRENT_SOURCE_DIR}/sum.comp
        -o ${CMAKE_BINARY_DIR}/sum.spv
        -DELT_COUNT=${ELT_COUNT}
        -DWORKGROUP_SIZE=${WORKGROUP_SIZE}
)

add_custom_target(
    shaders ALL
    DEPENDS sum.spv
)

add_executable(sum ${SOURCES})
add_dependencies(sum shaders)
