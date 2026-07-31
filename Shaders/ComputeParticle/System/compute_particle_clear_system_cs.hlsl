#include "compute_particle.hlsli"

RWByteAddressBuffer indirect_data_buffer : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    int index = dispatch_thread_id.x;
    indirect_data_buffer.Store(NumCurrentParticleOffset, 0);
    indirect_data_buffer.Store(NumPreviousParticleOffset, 0);
}
