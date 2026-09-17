#include "compute_particle.hlsli"

RWByteAddressBuffer indirect_data_buffer : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    //  前フレームの総パーティクル数
    uint num_previous_particle = indirect_data_buffer.Load(NumCurrentParticleOffset);
    
    //  「発生予定の」総パーティクル数
    uint total_spawn_count = total_cpu_spawn_count;

    //  「仮の」総パーティクル数
    uint num_current_particle = min(num_previous_particle + total_spawn_count, total_particle_count);

    //  格納
    indirect_data_buffer.Store(NumCurrentParticleOffset, num_current_particle);

    //	イベント用に発生数を加算
    indirect_data_buffer.Store(NumPreviousParticleOffset, num_previous_particle + total_emitter_spawn_count);
    
    //  このタイミングで更新用ディスパッチ数を決めておく
    float x = ceil((float) num_current_particle / (float) NumDispatchThread);
    uint3 dispatch_indirect = uint3((uint(x + 0.5f)), 1, 1);
    indirect_data_buffer.Store3(UpdateParticleDispatchIndirect, dispatch_indirect);
}
