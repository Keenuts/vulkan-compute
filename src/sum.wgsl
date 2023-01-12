@group(0) @binding(0) var<storage, read> buffer_in : array<i32>;
@group(0) @binding(1) var<storage, read_write> buffer_out : array<i32>;

@compute @workgroup_size(32, 1, 1)
fn main(@builtin(global_invocation_id) threadID : vec3<u32>) {
    if (threadID.x >= 1024) {
        return;
    }

    let id : u32 = threadID.x;
    buffer_out[id] = buffer_in[id] + buffer_in[id];
}
