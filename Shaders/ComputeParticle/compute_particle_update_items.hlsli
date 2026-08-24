#ifndef __COMPUTE_PARTICLES_UPDATE_ITEMS_HLSLI__
#define __COMPUTE_PARTICLES_UPDATE_ITEMS_HLSLI__

#include "compute_particle.hlsli"

//	汎用情報定義
RWStructuredBuffer<particle_header> particle_header_buffer : register(u0);
RWByteAddressBuffer particle_binary_buffer : register(u1);

StructuredBuffer<emitter_header> emitter_header_buffer : register(t0);
ByteAddressBuffer emitter_binary_buffer : register(t1);

#define SRV emitter_binary_buffer
#include "compute_particle_system_item_emitter.hlsli"

#define UAV particle_binary_buffer
#include "compute_particle_system_item_particle.hlsli"

////////////////////////////////////////////////////////////////////////////////////////////////////////
//  エミッターコンテキスト構築
emitter_context build_emitter_context(inout uint emitter_binary_offset, uint emitter_id, uint3 dispatch_thread_id)
{
    emitter_context em_context;

    em_common_item em_common;
    stream_common_item(rw_read_only, emitter_binary_offset, em_common);
    em_context.active_items = em_common.active_items;
    em_context.random_seed = generate_random_seed(dispatch_thread_id);
    em_context.elapsed_time = elapsed_time; //  とりあえずここで入れておく
    em_context.scale = em_common.scale;
    em_context.rotation = em_common.rotation;
    em_context.translation = em_common.translation;

    return em_context;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//  軌跡更新アイテム
void update_particle_trail_item(inout emitter_context em_context, inout pt_common_item pt_common, inout uint emitter_binary_offset, inout uint particle_binary_offset)
{
    if (has_flag(em_context.active_items, emitter_active_trail_item_bit_flag))
    {
        //  軌跡のヘッダーを取得
        pt_trail_header_item trail_header;
        uint header_write_offset = particle_binary_offset;
        stream_trail_header(rw_read, particle_binary_offset, trail_header);
        //  エミッター側の情報を取得
        {
            em_trail_item trail_item;
            stream_trail_item(rw_read_only, emitter_binary_offset, trail_item);
            if (has_flag(pt_common.state, particle_binary_state_bit_spawn_frame))
            {
                //  最初のフレームは特に何もしない
                trail_header.counter = 1;
                stream_trail_header(rw_write, header_write_offset, trail_header);
                //  ボディー初期化
                pt_trail_body_item trail_body;
                trail_body.translation = (float3) 0;
                trail_body.rotation = float4(0, 0, 0, 1);
                trail_body.scale = (float3) 0;
                uint write_offset = particle_binary_offset;
                [loop] for (uint index = 0; index < trail_item.max; ++index)
                {
                    stream_trail_body(rw_write, write_offset, trail_body);
                }
            }
            else
            {
                //  ヘッダー書き込み
                trail_header.counter = min(trail_header.counter + 1, trail_item.max);
                stream_trail_header(rw_write, header_write_offset, trail_header);

                //  ボディーに古い情報を書き込み
                [loop] for (int index = (int) trail_header.counter - 1; index > 0; --index)
                {
                    pt_trail_body_item trail_body;
                    uint read_offset = particle_binary_offset + (index - 1) * pt_trail_body_size_of_bytes;
                    stream_trail_body(rw_read, read_offset, trail_body);
                    uint write_offset = particle_binary_offset + index * pt_trail_body_size_of_bytes;
                    stream_trail_body(rw_write, write_offset, trail_body);
                }
            }

            //  ボディーの先頭に現在座標等を登録
            {
                pt_trail_body_item trail_body;
                trail_body.scale = pt_common.scale;
                trail_body.rotation = pt_common.rotation;
                trail_body.translation = pt_common.translation;

                uint write_offset = particle_binary_offset;
                stream_trail_body(rw_write, write_offset, trail_body);
            }
                
            //  最大サイズ分オフセット
            particle_binary_offset += trail_item.max * pt_trail_body_size_of_bytes;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//  スプライトアニメーション更新アイテム
void update_uv_item(inout emitter_context em_context, inout pt_common_item pt_common, inout uint emitter_binary_offset, inout uint particle_binary_offset)
{
    if (has_flag(em_context.active_items, emitter_active_uv_item_bit_flag))
    {
        //  表示用の物なのでエミッター分オフセットしておく
        em_uv_item em_uv;
        stream_uv_item(rw_read_only, emitter_binary_offset, em_uv);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//  速度更新アイテム
void update_particle_velocity_item(inout emitter_context em_context, inout pt_common_item pt_common, inout uint emitter_binary_offset, inout uint particle_binary_offset)
{
    if (has_flag(em_context.active_items, emitter_active_veloctiy_item_bit_flag))
    {
        pt_velocity_item pt_velocity;
        uint write_offset = particle_binary_offset;
        stream_velocity_item(rw_read, particle_binary_offset, pt_velocity);
        //  エミッター側の情報を取得
        {
            em_veloctiy_item velocity;
            stream_veloctiy_item(rw_read_only, emitter_binary_offset, velocity);
            if (has_flag(pt_common.state, particle_binary_state_bit_spawn_frame))
            {
                //  強さを乱数で
                float power = velocity.power_range.x;
                if (has_flag(velocity.state, emitter_veloctiy_item_state_random_power))
                    power = lerp(power, velocity.power_range.y, x_or_shift_random(em_context.random_seed));

                //  射出方向をランダムに
                float3 spread_dir = velocity.direction;
                if (has_flag(velocity.state, emitter_veloctiy_item_state_random_direction))
                {
                    float theta = lerp(velocity.angle_range.x, velocity.angle_range.y, x_or_shift_random(em_context.random_seed));
                    float phi = x_or_shift_random(em_context.random_seed) * PI;

                    float3 base_dir = normalize(velocity.direction);
                    float3 up = (abs(base_dir.y) < 0.99f) ? float3(0, 1, 0) : float3(1, 0, 0);
                    float3 tangent = normalize(cross(up, base_dir));
                    float3 bitangent = cross(base_dir, tangent);

                    float sin_theta, cos_theta;
                    sincos(theta, sin_theta, cos_theta);
                    float sin_phi, cos_phi;
                    sincos(phi, sin_phi, cos_phi);

                    spread_dir = base_dir * cos_theta + (tangent * cos_phi + bitangent * sin_phi) * sin_theta;
                }
                pt_velocity.velocity = spread_dir * power;
            }
            else
            {
                //  加速処理
                float3 acceleration = velocity.acceleration_min;
                if (has_flag(velocity.state, emitter_veloctiy_item_state_random_acceleration))
                {
                    acceleration = lerp(velocity.acceleration_min,
                                        velocity.acceleration_max,
                                        x_or_shift_random(em_context.random_seed));
                }
                pt_velocity.velocity += (velocity.gravity + acceleration) * em_context.elapsed_time;

                //  減速処理
                float3 deceleration = velocity.deceleration_min;
                if (has_flag(velocity.state, emitter_veloctiy_item_state_random_deceleration))
                {
                    deceleration = lerp(velocity.deceleration_min,
                                        velocity.deceleration_max,
                                        x_or_shift_random(em_context.random_seed));
                }
                float3 decel_step = min(abs(pt_velocity.velocity), deceleration * em_context.elapsed_time);
                pt_velocity.velocity -= sign(pt_velocity.velocity) * decel_step;

                //  移動
                pt_common.translation += pt_velocity.velocity * em_context.elapsed_time;
            }
        }
        stream_velocity_item(rw_write, write_offset, pt_velocity);
    }
}
#endif  //  __COMPUTE_PARTICLES_UPDATE_ITEMS_HLSLI__
