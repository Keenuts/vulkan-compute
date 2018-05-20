#version 450

layout (
    local_size_x = WORKGROUP_SIZE,
    local_size_y = 1,
    local_size_z = 1
) in;

layout (binding = 0) buffer buf_in  { int buffer_in[]; };
layout (binding = 1) buffer buf_out { int buffer_out[]; };

void main()
{
    if (gl_GlobalInvocationID.x >= ELT_COUNT)
        return;

    uint id = gl_GlobalInvocationID.x;
    buffer_out[id] = buffer_in[id] + buffer_in[id];
}
