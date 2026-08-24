#include "compute_particle.hlsli"

RWByteAddressBuffer indirect_data_buffer : register(u0);

StructuredBuffer<uint> emitter_table_buffer : register(t0);
StructuredBuffer<emitter_data> emitter_data_buffer : register(t1);

[numthreads(1, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    //  現フレームの総パーティクル数を算出して加算
    uint num_particle = 0;
    for (uint index = 0; index < total_emitter_count; ++index)
    {
        uint emitter_index = emitter_table_buffer[index];
        num_particle += (emitter_index == invalid_tag) ? 0 : emitter_data_buffer[emitter_index].particle_num;
    }
    indirect_data_buffer.Store(NumCurrentParticleOffset, num_particle);
}
