struct S {
  int value;
};

[[vk::push_constant]] S input;

RWStructuredBuffer<uint4> buffer_in;
RWStructuredBuffer<uint4> buffer_out;

[numthreads(2,2,2)]
void main(uint3 DTID : SV_DispatchThreadID, uint3 GID : SV_GroupID, uint3 GTID : SV_GroupThreadID, uint GI : SV_GroupIndex) {
  const uint3 DISPATCH_SIZE = uint3(2, 2, 2);
  const uint3 NUMTHREADS    = uint3(2, 2, 2);

  // Computing the global threads index.
  const uint THREAD_PER_LINE = NUMTHREADS.x * DISPATCH_SIZE.x;
  const uint THREAD_PER_PLANE = THREAD_PER_LINE * NUMTHREADS.y * DISPATCH_SIZE.y;
  const uint GX = GID.x * NUMTHREADS.x + GTID.x;
  const uint GY = GID.y * NUMTHREADS.y + GTID.y;
  const uint GZ = GID.z * NUMTHREADS.z + GTID.z;

  // Computing the flat global index.
  const uint FID = GZ * THREAD_PER_PLANE + GY * THREAD_PER_LINE + GX;

  // Values will le randomly located in the buffer due to the dispatch
  // order. And due to yaml formatting, we cannot rely on a per-tuple
  // key to to the unordered check.
  // Solution is to make all keys independent by having the MSB carry
  // the unique key, and the LSB the value to check.
  buffer_out[FID * 3 + 0] = uint4(
    ((FID * 12 +  0) << 16) | FID,
    ((FID * 12 +  1) << 16) | GID.x,
    ((FID * 12 +  2) << 16) | GID.y,
    ((FID * 12 +  3) << 16) | GID.z);
  buffer_out[FID * 3 + 1] = uint4(
    ((FID * 12 +  4) << 16) | GI,
    ((FID * 12 +  5) << 16) | GTID.x,
    ((FID * 12 +  6) << 16) | GTID.y,
    ((FID * 12 +  7) << 16) | GTID.z);
  buffer_out[FID * 3 + 2] = uint4(
    ((FID * 12 +  8) << 16) | 0,
    ((FID * 12 +  9) << 16) | DTID.x,
    ((FID * 12 + 10) << 16) | DTID.y,
    ((FID * 12 + 11) << 16) | DTID.z);
}
