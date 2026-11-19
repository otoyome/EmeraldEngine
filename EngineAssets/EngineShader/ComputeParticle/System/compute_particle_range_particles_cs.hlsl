#include "compute_particle.hlsli"

//	汎用情報定義
RWStructuredBuffer<emitter_range> emitter_range_buffer : register(u0);

StructuredBuffer<particle_header> particle_header_buffer : register(t0);

cbuffer RANGE_PARTICLES_CONSTANTS : register(b11)
{
    uint range_particle_offset_index;
    uint range_particle_dummy[3];
};

[numthreads(NumDispatchThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    //  パーティクル番号
    uint particle_index_header_first, particle_index_header_second;
    particle_index_header_first = dispatch_thread_id.x * 2 + 0 + range_particle_offset_index;
    particle_index_header_second = dispatch_thread_id.x * 2 + 1 + range_particle_offset_index;

    if (total_particle_count <= particle_index_header_second)
        return;

    //  パーティクル情報
    particle_header particle_header_first, particle_header_second;
    particle_header_first = particle_header_buffer.Load(particle_index_header_first);
    particle_header_second = particle_header_buffer.Load(particle_index_header_second);

    //  データ的な終端判定
    if (particle_header_first.tag == invalid_tag && particle_header_second.tag == invalid_tag)
        return;

    //  パーティクルのステータス取得
    bool alive_first, alive_second;
    uint emitter_id_first, emitter_id_second;
    uint particle_index_first, particle_index_second;
    get_particle_header_parameter(particle_header_first, alive_first, emitter_id_first, particle_index_first);

    //  終端判定
    if (particle_header_first.tag != invalid_tag && particle_header_second.tag == invalid_tag)
    {
        if (alive_first)
        {
            emitter_range_buffer[emitter_id_first].alive_end = particle_index_header_second;

        }
        else
        {
            emitter_range_buffer[emitter_id_first].dead_end = particle_index_header_second;
        }
        return;
    }

    //  2要素ずつ比較
    get_particle_header_parameter(particle_header_second, alive_second, emitter_id_second, particle_index_second);

    if (alive_first && alive_second)
    {
        //  両方生存
        if (emitter_id_first != emitter_id_second)
        {
            emitter_range_buffer[emitter_id_first].alive_end = particle_index_header_second;
            emitter_range_buffer[emitter_id_second].alive_head = particle_index_header_second;
        }
    }
    else if (alive_first && !alive_second)
    {
        //  生死ライン
        emitter_range_buffer[emitter_id_first].alive_end = particle_index_header_second;
        emitter_range_buffer[emitter_id_second].dead_head = particle_index_header_second;
    }
    else
    {
        //  両方死亡
        if (emitter_id_first != emitter_id_second)
        {
            emitter_range_buffer[emitter_id_first].dead_end = particle_index_header_second;
            emitter_range_buffer[emitter_id_second].dead_head = particle_index_header_second;
        }
    }
}
