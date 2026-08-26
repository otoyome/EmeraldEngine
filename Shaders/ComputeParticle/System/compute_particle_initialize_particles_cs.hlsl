#include "compute_particle.hlsli"

//	汎用情報定義
RWStructuredBuffer<particle_header> particle_header_buffer : register(u0);
RWStructuredBuffer<emitter_data> emitter_data_buffer : register(u1);
RWStructuredBuffer<uint> particle_index_buffer : register(u2);
RWByteAddressBuffer particle_binary_buffer : register(u3);

StructuredBuffer<uint> emitter_table_buffer : register(t0);
StructuredBuffer<emitter_header> emitter_header_buffer : register(t1);
ByteAddressBuffer emitter_binary_buffer : register(t2);

#define SRV emitter_binary_buffer
#include "compute_particle_system_item_emitter.hlsli"

#define UAV particle_binary_buffer
#include "compute_particle_system_item_particle.hlsli"

[numthreads(1, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint emitter_index = emitter_table_buffer.Load(dispatch_thread_id.x);
    if (emitter_index == invalid_tag)
        return;

    emitter_data em_data = emitter_data_buffer[emitter_index];
    if (em_data.spawn_num == 0)
        return;

    emitter_header em_header = emitter_header_buffer[emitter_index];

    //  エミッターバイナリ座標を特定
    uint emitter_binary_offset = em_header.emitter_binary_head;

    //  エミッター共通情報取得
    em_common_item em_common;
    stream_common_item(rw_read_only, emitter_binary_offset, em_common);
    if (has_flag(em_common.active_items, emitter_active_trail_item_bit_flag))
    {
        em_trail_item em_trail = (em_trail_item) 0;
        stream_trail_item(rw_read_only, emitter_binary_offset, em_trail);
    }
    if (has_flag(em_common.active_items, emitter_active_uv_item_bit_flag))
    {
        em_uv_item em_uv = (em_uv_item) 0;
        stream_uv_item(rw_read_only, emitter_binary_offset, em_uv);
    }
    em_color_item em_color = (em_color_item) 0;
    if (has_flag(em_common.active_items, emitter_active_color_item_bit_flag))
    {
        stream_color_item(rw_read_only, emitter_binary_offset, em_color);
    }

    //  パーティクルインデックスのヘッダー
    uint particle_index_head = em_header.particle_index_head;
    particle_index_head += em_data.particle_num;

    //  乱数の種を生成
    x_or_shift_128_state x_or_shift_random_seed = generate_random_seed(dispatch_thread_id);
    
    //  パーティクルヘッダー情報設定
    [loop] for (uint index = 0; index < em_data.spawn_num; ++index)
    {
        //  ヘッダーとして利用する番号を取得
        uint particle_header_index = em_data.spawn_head + index;

        //  パーティクル番号を取得
        uint particle_index = particle_index_buffer[particle_index_head + index];

        //  取得したパーティクル番号が違反タグならパーティクルインデックス生成
        if (particle_index == invalid_tag)
        {
            particle_index = em_data.particle_num + index;
        }

        //  パーティクルの共通情報初期化
        {
            uint seed = particle_index;
            pt_common_item pt_common;
            pt_common.state = particle_binary_state_bit_spawn_frame;
            pt_common.life_time = 0.0f;
            pt_common.life_time_max = lerp(em_common.spawn_life_time_range.x, em_common.spawn_life_time_range.y, x_or_shift_random(x_or_shift_random_seed));
            float scale_value = lerp(em_common.spawn_scale_range.x, em_common.spawn_scale_range.y, x_or_shift_random(x_or_shift_random_seed));
            pt_common.scale = em_common.scale * scale_value.xxx;
            pt_common.rotation = em_common.rotation;
            float3 min = em_common.translation + em_common.spawn_area_min;
            float3 max = em_common.translation + em_common.spawn_area_max;
            pt_common.translation = lerp(min, max, x_or_shift_random3(x_or_shift_random_seed));
            pt_common.color = 1;
            if (has_flag(em_common.active_items, emitter_active_color_item_bit_flag))
            {
                //  色のランダム生成
                if (has_flag(em_color.state, emitter_color_item_state_random))
                {
                    pt_common.color = lerp(em_color.color_min, em_color.color_max, x_or_shift_random4(x_or_shift_random_seed));
                }
                else
                {
                    pt_common.color = em_color.color_min;
                }
            }
            uint particle_binary_offset = em_header.particle_binary_head + em_header.particle_size * particle_index;
            stream_common_item(rw_write, particle_binary_offset, pt_common);
        }

        //  パーティクルヘッダー生成
        particle_header_buffer[particle_header_index] = generate_particle_header(emitter_index, particle_index);
        //  パーティクルインデックスバッファも更新
        particle_index_buffer[particle_index_head + index] = particle_index;
    }
}
