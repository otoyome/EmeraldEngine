#include "compute_particle.hlsli"

RWByteAddressBuffer indirect_data_buffer : register(u0);

RWStructuredBuffer<emitter_data> emitter_data_buffer : register(u1);

[numthreads(1, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    indirect_data_buffer.Store(NumCurrentParticleOffset, 0);
    indirect_data_buffer.Store(NumPreviousParticleOffset, 0);

    for (uint emitter_index = 0; emitter_index < total_emitter_count; ++emitter_index)
    {
        emitter_data_buffer[emitter_index].spawn_head = 0;
        emitter_data_buffer[emitter_index].spawn_num = 0;
        emitter_data_buffer[emitter_index].particle_num = 0;
        emitter_data_buffer[emitter_index].particle_max = 0;
    }
}
