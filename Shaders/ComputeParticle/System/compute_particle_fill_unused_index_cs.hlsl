#include "compute_particle.hlsli"

RWStructuredBuffer<uint> particle_index_buffer : register(u0);

cbuffer FILL_UNUSED_INDEX_CONSTANTS : register(b11)
{
    uint emitter_particle_index_head; // エミッター毎のパーティクル開始
    uint emitter_particle_index_end; // エミッター毎のパーティクル終端
    uint emitter_data_dummy[2];
};

[numthreads(NumDispatchThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    int index = emitter_particle_index_head + dispatch_thread_id.x;
    if (emitter_particle_index_end < index)
        return;

    particle_index_buffer[index] = invalid_tag;
}
