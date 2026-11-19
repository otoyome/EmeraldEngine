#include "compute_particle.hlsli"

//	汎用情報定義
RWStructuredBuffer<uint> particle_index_buffer : register(u0);
RWStructuredBuffer<emitter_data> emitter_data_buffer : register(u1);
RWByteAddressBuffer indirect_data_buffer : register(u2);

StructuredBuffer<uint> emitter_table_buffer : register(t0);
StructuredBuffer<emitter_header> emitter_header_buffer : register(t1);
StructuredBuffer<emitter_range> emitter_range_buffer : register(t2);
ByteAddressBuffer emitter_binary_buffer : register(t3);
StructuredBuffer<particle_header> particle_header_buffer : register(t4);

#define SRV emitter_binary_buffer
#include "compute_particle_system_item_emitter.hlsli"

//  エミッターの最大値
static const uint NumThread = 1024;

[numthreads(NumThread, 1, 1)]
void main(uint3 group_thread_ids : SV_GroupThreadID, uint group_index : SV_GroupIndex)
{
    if (group_thread_ids.x >= total_emitter_count)
        return;

    //  エミッター番号を取得
    uint emitter_id = emitter_table_buffer[group_thread_ids.x];
    if (emitter_id == invalid_tag)
        return;

    //  パーティクル総数更新
    emitter_range range = emitter_range_buffer[emitter_id];
    uint particle_num = range.alive_end - range.alive_head;

    //  エミッター情報取得
    em_common_item em_common;
    uint emitter_binary_offset = emitter_header_buffer[emitter_id].emitter_binary_head;
    stream_common_item(rw_read_only, emitter_binary_offset, em_common);

    //  頂点数計算
    uint vertex_count = 0;
    if (em_common.active_items & emitter_active_trail_item_bit_flag)
    {
        //  ribon
        em_trail_item trail_item;
        stream_trail_item(rw_read_only, emitter_binary_offset, trail_item);
        vertex_count = particle_num * trail_item.max * index_size;
    }
    else
    {
        //  sprite
        vertex_count = particle_num * index_size;
    }

    //  エミッター毎の描画回数の指定
    uint4 draw_indirect_data = uint4(vertex_count, 1, 0, 0);
    uint offset = FixedIndirectDataSize + emitter_id * draw_indirect_size;
    indirect_data_buffer.Store4(offset, draw_indirect_data);
    
    //  パーティクル総数更新
    emitter_data_buffer[emitter_id].particle_num = particle_num;
}
