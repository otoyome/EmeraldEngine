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
    em_context.random_seed = generate_random_seed(dispatch_thread_id, em_common.random_seed);
    em_context.elapsed_time = em_common.elapsed_time;  //  エミッター毎の経過時間を使う
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
                trail_header.counter = 2;
                stream_trail_header(rw_write, header_write_offset, trail_header);
                //  ボディー初期化
                pt_trail_body_item trail_body;
                trail_body.scale = pt_common.scale;
                trail_body.rotation = pt_common.rotation;
                trail_body.translation = pt_common.translation;
                uint write_offset = particle_binary_offset;
                [loop] for (uint index = 0; index < trail_item.max; ++index)
                {
                    stream_trail_body(rw_write, write_offset, trail_body);
                }
            }
            else if (em_context.elapsed_time > 0.0f)   //  移動中のみ処理
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

                //  ボディーの先頭に現在座標等を登録
                {
                    pt_trail_body_item trail_body;
                    trail_body.scale = pt_common.scale;
                    trail_body.rotation = pt_common.rotation;
                    trail_body.translation = pt_common.translation;

                    uint write_offset = particle_binary_offset;
                    stream_trail_body(rw_write, write_offset, trail_body);
                }
            }
                
            //  最大サイズ分オフセット
            particle_binary_offset += trail_item.max * pt_trail_body_size_of_bytes;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//  UV更新アイテム
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
//  色更新アイテム
void update_color_item(inout emitter_context em_context, inout pt_common_item pt_common, inout uint emitter_binary_offset, inout uint particle_binary_offset)
{
    if (has_flag(em_context.active_items, emitter_active_color_item_bit_flag))
    {
        //  表示用の物なのでエミッター分オフセットしておく
        em_color_item em_uv;
        stream_color_item(rw_read_only, emitter_binary_offset, em_uv);
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
                //  速度回転
                pt_velocity.velocity = rotate_vector_quaternion(spread_dir, em_context.rotation) * power;
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
                //  加速方向回転
                acceleration = rotate_vector_quaternion(acceleration, em_context.rotation);
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

////////////////////////////////////////////////////////////////////////////////////////////////////////
//  引き込み更新アイテム
void update_particle_attract_item(inout emitter_context em_context, inout pt_common_item pt_common, inout uint emitter_binary_offset, inout uint particle_binary_offset)
{
    if (has_flag(em_context.active_items, emitter_active_attract_item_bit_flag))
    {
        //  粒子側の蓄積速度
        pt_attract_item pt_attract;
        uint write_offset = particle_binary_offset;
        stream_attract_item(rw_read, particle_binary_offset, pt_attract);

        em_attract_item attract;
        stream_attract_item(rw_read_only, emitter_binary_offset, attract);

        if (has_flag(pt_common.state, particle_binary_state_bit_spawn_frame))
        {
            //  最初のフレームは初期化
            pt_attract.velocity = float3(0, 0, 0);
        }
        else if (em_context.elapsed_time > 0.0f)
        {
            float3 target_vector = attract.position - pt_common.translation;
            float dist = length(target_vector);

            if (dist > 0.0001f)
            {
                float3 dir = target_vector / dist;

                //  影響範囲による減衰
                float influence = saturate(1.0f - dist / max(attract.radius, 0.0001f));
                influence = pow(influence, max(attract.falloff, 0.0001f));

                //  加速処理
                pt_attract.velocity += dir * attract.strength * influence * em_context.elapsed_time;
                //  減速処理
                pt_attract.velocity *= max(1.0f - attract.damping * em_context.elapsed_time, 0.0f);
                //  移動
                pt_common.translation += pt_attract.velocity * em_context.elapsed_time;
            }

            //  消滅範囲に入ったら寿命切れ扱いで消す
            if (dist <= attract.kill_radius)
            {
                pt_common.life_time = pt_common.life_time_max;
            }
        }

        stream_attract_item(rw_write, write_offset, pt_attract);
    }
}
#endif  //  __COMPUTE_PARTICLES_UPDATE_ITEMS_HLSLI__
