RWStructuredBuffer<int> buffer_in;
RWStructuredBuffer<int> buffer_out;

[numthreads(WORKGROUP_SIZE,1,1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
  if (threadID.x >= 1024)
      return;

  const uint id = threadID.x;
  buffer_out[id] = buffer_in[id] + buffer_in[id];
}
