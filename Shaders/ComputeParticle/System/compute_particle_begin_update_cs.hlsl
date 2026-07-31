#include "compute_particle.hlsli"

RWByteAddressBuffer indirect_data_buffer : register(u0);

StructuredBuffer<uint> emitter_table_buffer : register(t0);
StructuredBuffer<emitter_data> emitter_data_buffer : register(t1);

[numthreads(1, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    //  前フレームの総パーティクル数
    uint num_previous_particle = 0;
    for (uint index = 0; index < total_emitter_count; ++index)
    {
        uint emitter_index = emitter_table_buffer[index];
        num_previous_particle += (emitter_index == invalid_tag) ? 0 : emitter_data_buffer[emitter_index].particle_num;
    }
    
    //  「発生予定の」総パーティクル数
    //  メッシュパーティクルは別途 indirect_data_buffer にてカウント予定
    uint total_spawn_count = total_cpu_spawn_count;

    //  「仮の」総パーティクル数
    uint num_current_particle = num_previous_particle + total_spawn_count;
    num_current_particle = min(num_current_particle, total_particle_count);

    //  格納
    indirect_data_buffer.Store(NumCurrentParticleOffset, num_current_particle);
    indirect_data_buffer.Store(NumPreviousParticleOffset, num_previous_particle);
    
    //  このタイミングで更新用ディスパッチ数を決めておく
    float x = ceil((float) num_current_particle / (float) NumDispatchThread);
    uint3 dispatch_indirect = uint3((uint(x + 0.5f)), 1, 1);
    indirect_data_buffer.Store3(UpdateParticleDispatchIndirect, dispatch_indirect);
}
