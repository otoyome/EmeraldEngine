#include "compute_particle.hlsli"

//	汎用情報定義
RWStructuredBuffer<uint> particle_index_buffer : register(u0);
RWStructuredBuffer<emitter_data> emitter_data_buffer : register(u1);
RWStructuredBuffer<particle_header> particle_header_buffer : register(u2);

StructuredBuffer<uint> emitter_table_buffer : register(t0);
StructuredBuffer<emitter_header> emitter_header_buffer : register(t1);
StructuredBuffer<emitter_range> emitter_range_buffer : register(t2);

[numthreads(NumDispatchThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint header_index = dispatch_thread_id.x;
    if (header_index >= total_particle_count)
        return;

    particle_header header = particle_header_buffer[header_index];
    if (header.tag == invalid_tag)
        return;
    
    bool alive;
    uint emitter_id, particle_index;
    get_particle_header_parameter(header, alive, emitter_id, particle_index);

    emitter_range range = emitter_range_buffer[emitter_id];
    uint particle_index_head = emitter_header_buffer[emitter_id].particle_index_head;

    if (header_index >= range.alive_head && header_index < range.alive_end)
    {
        //  自分の位置から直接オフセット算出
        uint index = particle_index_head + (header_index - range.alive_head);
        particle_index_buffer[index] = particle_index;
    }
    else if (header_index >= range.dead_head && header_index < range.dead_end)
    {
        //  alive分だけずらして算出
        uint alive_count = range.alive_end - range.alive_head;
        uint index = particle_index_head + alive_count + (header_index - range.dead_head);
        particle_index_buffer[index] = particle_index;

        //  破棄情報として利用
        particle_header dead_header;
        dead_header.tag = invalid_tag;
        dead_header.depth = 0;
        particle_header_buffer[header_index] = dead_header;
    }
}
