#include "compute_particle.hlsli"

RWStructuredBuffer<particle_header> particle_header_buffer : register(u0);
RWStructuredBuffer<emitter_data> emitter_data_buffer : register(u1);
RWStructuredBuffer<uint> particle_index_buffer : register(u2);
RWByteAddressBuffer particle_binary_buffer : register(u3);
RWByteAddressBuffer indirect_data_buffer : register(u4);

StructuredBuffer<emitter_header> emitter_header_buffer : register(t0);
ByteAddressBuffer emitter_binary_buffer : register(t1);
StructuredBuffer<particle_event> particle_event_buffer : register(t2);
ByteAddressBuffer particle_event_count_buffer : register(t3);

#define SRV emitter_binary_buffer
#include "compute_particle_system_item_emitter.hlsli"

#define UAV particle_binary_buffer
#include "compute_particle_system_item_particle.hlsli"

[numthreads(NumDispatchThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint event_count = particle_event_count_buffer.Load(0);
    if (dispatch_thread_id.x >= event_count)
        return;

    particle_event ev = particle_event_buffer[dispatch_thread_id.x];

    uint emitter_index = ev.target_emitter_id;
    if (emitter_index >= total_emitter_count)
        return;

    emitter_data em_data = emitter_data_buffer[emitter_index];
    if (em_data.particle_max == 0)
        return;

    emitter_header em_header = emitter_header_buffer[emitter_index];

    //  エミッター共通アイテム
    uint emitter_binary_offset = em_header.emitter_binary_head;
    em_common_item em_common;
    stream_common_item(rw_read_only, emitter_binary_offset, em_common);

    //  パーティクルヘッダースロットを予約
    uint header_index = 0;
    indirect_data_buffer.InterlockedAdd(NumPreviousParticleOffset, 1, header_index);
    uint current_particle_count = indirect_data_buffer.Load(NumCurrentParticleOffset);
    if (header_index >= current_particle_count)
        return;

    //  対象エミッター内のスロットを予約
    uint spawn_offset = 0;
    InterlockedAdd(emitter_data_buffer[emitter_index].spawn_num, 1, spawn_offset);
    uint local_index = em_data.particle_num + spawn_offset;
    if (local_index >= em_data.particle_max)
        return;

    //  パーティクル番号を取得
    uint particle_index_slot = em_header.particle_index_head + local_index;
    uint particle_index = particle_index_buffer[particle_index_slot];
    if (particle_index == invalid_tag)
        particle_index = local_index;

    //  乱数の種
    x_or_shift_128_state random_seed = generate_random_seed(uint3(header_index, 0, 0), em_common.random_seed + dispatch_thread_id.x);

    //  パーティクル共通情報を初期化
    pt_common_item pt_common;
    pt_common.state = particle_binary_state_bit_spawn_frame;
    pt_common.life_time = 0.0f;
    pt_common.life_time_max = lerp(em_common.spawn_life_time_range.x, em_common.spawn_life_time_range.y, x_or_shift_random(random_seed));
    float scale_value = lerp(em_common.spawn_scale_range.x, em_common.spawn_scale_range.y, x_or_shift_random(random_seed));
    pt_common.scale = em_common.scale * scale_value.xxx;
    pt_common.rotation = em_common.rotation;
    pt_common.translation = ev.position;   //  イベント発生位置
    pt_common.color = 1;

    uint particle_binary_offset = em_header.particle_binary_head + em_header.particle_size * particle_index;
    stream_common_item(rw_write, particle_binary_offset, pt_common);

    //  ヘッダーと番号を書き込み
    particle_header_buffer[header_index] = generate_particle_header(emitter_index, particle_index);
    particle_index_buffer[particle_index_slot] = particle_index;
}
