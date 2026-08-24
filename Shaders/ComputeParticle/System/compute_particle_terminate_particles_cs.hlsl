#include "compute_particle.hlsli"

//	汎用情報定義
RWStructuredBuffer<uint> particle_index_buffer : register(u0);
RWStructuredBuffer<emitter_data> emitter_data_buffer : register(u1);
RWStructuredBuffer<particle_header> particle_header_buffer : register(u2);

StructuredBuffer<uint> emitter_table_buffer : register(t0);
StructuredBuffer<emitter_header> emitter_header_buffer : register(t1);
StructuredBuffer<emitter_range> emitter_range_buffer : register(t2);

static const uint ThreadsCount = 1024;

[numthreads(ThreadsCount, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= total_emitter_count)
        return;

    uint emitter_index = emitter_table_buffer.Load(dispatch_thread_id.x);
    if (emitter_index == invalid_tag)
        return;

    //  死亡しているパーティクル番号をパーティクルインデックスバッファに返す
    emitter_range range = emitter_range_buffer[emitter_index];

    //  パーティクル総数更新
    emitter_data_buffer[emitter_index].particle_num = range.alive_end - range.alive_head;

    //  パーティクル破棄
    particle_header dead_header;
    dead_header.tag = invalid_tag;
    dead_header.depth = 0;

    //  死亡したパーティクル番号をインデックスバッファに格納
    uint particle_index_head = emitter_header_buffer[emitter_index].particle_index_head;
    bool alive;
    uint emitter_id, particle_index;

    //  生存者の番号を詰めていく
    uint index, index_buffer_counter = particle_index_head;
    for (index = range.alive_head; index < range.alive_end; ++index)
    {
        particle_header header = particle_header_buffer[index];
        get_particle_header_parameter(header, alive, emitter_id, particle_index);

        particle_index_buffer[index_buffer_counter] = particle_index;
        ++index_buffer_counter;
    }
    //  破棄者の番号を詰めていく
    for (index = range.dead_head; index < range.dead_end; ++index)
    {
        particle_header header = particle_header_buffer[index];
        get_particle_header_parameter(header, alive, emitter_id, particle_index);
        if (header.tag == invalid_tag)
            continue;

        particle_index_buffer[index_buffer_counter] = particle_index;
        particle_header_buffer[index] = dead_header;
        ++index_buffer_counter;
    }
}
