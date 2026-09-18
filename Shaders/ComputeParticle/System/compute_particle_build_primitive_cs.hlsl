#include "compute_particle.hlsli"

//	汎用情報定義
static const uint vertex_buffer_max = 4;
RWByteAddressBuffer vertex_buffer[vertex_buffer_max] : register(u0);

StructuredBuffer<emitter_range> emitter_range_buffer : register(t0);
StructuredBuffer<emitter_header> emitter_header_buffer : register(t1);
ByteAddressBuffer emitter_binary_buffer : register(t2);
StructuredBuffer<particle_header> particle_header_buffer : register(t3);
ByteAddressBuffer particle_binary_buffer : register(t4);

#define SRV emitter_binary_buffer
#include "compute_particle_system_item_emitter.hlsli"

#define SRV particle_binary_buffer
#include "compute_particle_system_item_particle.hlsli"

cbuffer BUILD_PRIMITIVE_CONSTANTS : register(b11)
{
    uint4 emitter_ids[vertex_buffer_max / 4];
};

//  ShaderModel5.0ではこうしないとできないので……辛み
void store2(uint vertex_buffer_index, inout uint vertex_buffer_byte_offset, in uint2 value)
{
    switch (vertex_buffer_index)
    {
        case 0: vertex_buffer[0].Store2(vertex_buffer_byte_offset, value); break;
        case 1: vertex_buffer[1].Store2(vertex_buffer_byte_offset, value); break;
        case 2: vertex_buffer[2].Store2(vertex_buffer_byte_offset, value); break;
        default: vertex_buffer[3].Store2(vertex_buffer_byte_offset, value); break;
    }
    vertex_buffer_byte_offset += 4 * 2;
}
void store3(uint vertex_buffer_index, inout uint vertex_buffer_byte_offset, in uint3 value)
{
    switch (vertex_buffer_index)
    {
        case 0: vertex_buffer[0].Store3(vertex_buffer_byte_offset, value); break;
        case 1: vertex_buffer[1].Store3(vertex_buffer_byte_offset, value); break;
        case 2: vertex_buffer[2].Store3(vertex_buffer_byte_offset, value); break;
        default: vertex_buffer[3].Store3(vertex_buffer_byte_offset, value); break;
    }
    vertex_buffer_byte_offset += 4 * 3;
}
void store4(uint vertex_buffer_index, inout uint vertex_buffer_byte_offset, in uint4 value)
{
    switch (vertex_buffer_index)
    {
        case 0: vertex_buffer[0].Store4(vertex_buffer_byte_offset, value); break;
        case 1: vertex_buffer[1].Store4(vertex_buffer_byte_offset, value); break;
        case 2: vertex_buffer[2].Store4(vertex_buffer_byte_offset, value); break;
        default: vertex_buffer[3].Store4(vertex_buffer_byte_offset, value); break;
    }
    vertex_buffer_byte_offset += 4 * 4;
}

//  頂点登録
void register_vertex(uint vertex_buffer_index, inout uint vertex_buffer_byte_offset, in float4 vertex_positions[4], in float4 vertex_color[4], in float2 vertex_texcoord[4])
{
    static const uint index_list[index_size] = { 0, 1, 2, 1, 3, 2 };
    [loop]
    for (uint i = 0; i < index_size; i++)
    {
        uint index = index_list[i];
        float3 position = vertex_positions[index].xyz;
        float4 color = vertex_color[index];
        float2 texcoord = vertex_texcoord[index];
        store3(vertex_buffer_index, vertex_buffer_byte_offset, asuint(position));
        store4(vertex_buffer_index, vertex_buffer_byte_offset, asuint(color));
        store2(vertex_buffer_index, vertex_buffer_byte_offset, asuint(texcoord));
    }
}

//  UV矩形生成
float4 calculate_uv_rect(in pt_common_item pt_common, in em_uv_item em_uv)
{
    float4 rect = em_uv.rect;
    if (em_uv.state & emitter_uv_item_state_direct)
    {
        //  アニメーション制御
        if (em_uv.state & emitter_uv_item_state_animation)
        {
            float speed = pt_common.life_time * em_uv.time;
            float2 direction = float2(em_uv.split_count.x, em_uv.split_count.y);
            direction -= emitter_uv_item_animation_direction_basis_value;
            direction = normalize(direction);
            rect.x += direction.x * speed;
            rect.y += direction.y * speed;
            rect.z += direction.x * speed;
            rect.w += direction.y * speed;
        }
    }
    else
    {
        //  分割数
        uint sx = max(em_uv.split_count.x, 1);
        uint sy = max(em_uv.split_count.y, 1);

        //  UV矩形設定
        float w = rect.z - rect.x;
        float h = rect.w - rect.y;
        float u = w / (float) sx;
        float v = h / (float) sy;

        //  番号指定
        int uv_index = em_uv.index;

        //  アニメーション制御
        if (em_uv.state & emitter_uv_item_state_animation)
        {
            uv_index += (pt_common.life_time / em_uv.time);
        }
        
        //  リピート
        uv_index %= sx * sy;

        //  UV算出
        rect.x += u * (uv_index % sx);
        rect.y += v * (uv_index / sx);
        rect.z = u;
        rect.w = v;
    }
    return rect;
}

//  リボンの幅方向を求める
float3 calculate_trail_axis(float3 pos0, float3 pos1, float3 face_dir)
{
    float3 axis = float3(-1, 0, 0);
    float3 segment = pos1 - pos0;
    if (length(segment) > 0.001f)
    {
        axis = cross(normalize(segment), normalize(face_dir));
    }
    return axis;
}

//  経過時間によるフェードイン・フェードアウト係数を求める
float calculate_trail_time_fade(float life_time, float life_time_max, float2 time_fade_rate)
{
    float time_rate = saturate(life_time / life_time_max);
    float fade_in = smoothstep(0.0f, time_fade_rate.x, time_rate);
    float fade_out = 1.0f - smoothstep(time_fade_rate.y, 1.0f, time_rate);
    return fade_in * fade_out;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(NumDispatchThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= total_particle_count)
        return;

    //  パーティクルヘッダーから情報取得
    bool alive;
    uint emitter_id, particle_index;
    particle_header header = particle_header_buffer.Load(dispatch_thread_id.x);
    get_particle_header_parameter(header, alive, emitter_id, particle_index);
    if (!alive)
        return;

    //  エミッター処理範囲か判定
    uint target_vertex_buffer_index = -1;
    [unroll]
    for (uint index = 0; index < vertex_buffer_max / 4; ++index)
    {
        if (emitter_ids[index].x == emitter_id)
        {
            target_vertex_buffer_index = 0;
        }
        if (emitter_ids[index].y == emitter_id)
        {
            target_vertex_buffer_index = 1;
        }
        if (emitter_ids[index].z == emitter_id)
        {
            target_vertex_buffer_index = 2;
        }
        if (emitter_ids[index].w == emitter_id)
        {
            target_vertex_buffer_index = 3;
        }
    }
    if (target_vertex_buffer_index == -1)
        return;

    //  バイナリ座標を特定
    uint emitter_binary_offset = emitter_header_buffer[emitter_id].emitter_binary_head;
    uint particle_binary_offset = emitter_header_buffer[emitter_id].particle_size * particle_index;
    uint particle_header_offset = dispatch_thread_id.x - emitter_range_buffer[emitter_id].alive_head;
    particle_binary_offset += emitter_header_buffer[emitter_id].particle_binary_head;

    //  エミッター情報取得
    em_common_item em_common = (em_common_item)0;
    em_trail_item em_trail = (em_trail_item) 0;
    em_uv_item em_uv = (em_uv_item) 0;
    em_color_item em_color = (em_color_item) 0;
    {
        stream_common_item(rw_read_only, emitter_binary_offset, em_common);
        if (em_common.active_items & emitter_active_trail_item_bit_flag)
        {
            stream_trail_item(rw_read_only, emitter_binary_offset, em_trail);
        }
        if (em_common.active_items & emitter_active_uv_item_bit_flag)
        {
            stream_uv_item(rw_read_only, emitter_binary_offset, em_uv);
        }
        if (em_common.active_items & emitter_active_color_item_bit_flag)
        {
            stream_color_item(rw_read_only, emitter_binary_offset, em_color);
        }

        //  以降は不要
    }

    //  パーティクル共通情報取得
    pt_common_item pt_common = (pt_common_item) 0;
    pt_trail_header_item pt_trail_header = (pt_trail_header_item) 0;
    {
        stream_common_item(rw_read_only, particle_binary_offset, pt_common);
        if (em_common.active_items & emitter_active_trail_item_bit_flag)
        {
            stream_trail_header(rw_read_only, particle_binary_offset, pt_trail_header);
        }

        //  以降は不要
    }

    //  軌跡形状アイテムを取得しているか判定
    float4 vertex_color[4], vertex_positions[4];
    float2 vertex_texcoord[4];
    if (em_common.active_items & emitter_active_trail_item_bit_flag)
    {
        //  プリミティブ生成
        uint read_offset = 0;

        //  ビルボード無効時は軌跡の面をパーティクルの姿勢で決める
        float3 trail_face_dir = -camera_direction.xyz;
        if (has_flag(em_common.emitter_state, em_common_item_state_billboard_off))
        {
            trail_face_dir = rotate_vector_quaternion(float3(0, 1, 0), pt_common.rotation);
        }

        //  初期値設定
        pt_trail_body_item trail_body[2];
        {
            read_offset = particle_binary_offset + pt_trail_body_size_of_bytes;
            stream_trail_body(rw_read_only, read_offset, trail_body[0]);
            stream_trail_body(rw_read_only, read_offset, trail_body[1]);

            float3 x_axis = calculate_trail_axis(trail_body[0].translation, trail_body[1].translation, trail_face_dir);
            vertex_positions[2] = float4(trail_body[0].translation + x_axis * trail_body[0].scale, 1);
            vertex_positions[3] = float4(trail_body[0].translation - x_axis * trail_body[0].scale, 1);
        }
        
        //  UV矩形
        float4 uv_rect = float4(0, 0, 1, 1);
        if (em_common.active_items & emitter_active_uv_item_bit_flag)
        {
            uv_rect = calculate_uv_rect(pt_common, em_uv);
        }
        
        //  頂点登録
        uint index = 0;
        [loop] for (index = 0; index < pt_trail_header.counter; ++index)
        {
            vertex_positions[0] = vertex_positions[2];
            vertex_positions[1] = vertex_positions[3];

            uint next_index = min(index + 1, em_trail.max - 1);
            uint next_next_index = min(index + 2, em_trail.max - 1);
            read_offset = particle_binary_offset + next_index * pt_trail_body_size_of_bytes;
            stream_trail_body(rw_read_only, read_offset, trail_body[0]);
            read_offset = particle_binary_offset + next_next_index * pt_trail_body_size_of_bytes;
            stream_trail_body(rw_read_only, read_offset, trail_body[1]);

            //  座標計算
            float3 x_axis = calculate_trail_axis(trail_body[0].translation, trail_body[1].translation, trail_face_dir);
            vertex_positions[2] = float4(trail_body[0].translation + x_axis * trail_body[0].scale, 1);
            vertex_positions[3] = float4(trail_body[0].translation - x_axis * trail_body[0].scale, 1);
            vertex_positions[2] = (vertex_positions[0] + vertex_positions[2]) * 0.5f;
            vertex_positions[3] = (vertex_positions[1] + vertex_positions[3]) * 0.5f;

            {
                float rate[2] = { saturate((float) (index + 0) / em_trail.max), saturate((float) (index + 1) / em_trail.max) };
                vertex_color[0] = pt_common.color;
                vertex_color[1] = pt_common.color;
                vertex_color[2] = pt_common.color;
                vertex_color[3] = pt_common.color;

                //  末尾に行くほどフェード
                if (em_trail.state & em_trail_item_state_use_fade)
                {
                    vertex_color[0].a *= 1.0f - rate[0];
                    vertex_color[1].a *= 1.0f - rate[0];
                    vertex_color[2].a *= 1.0f - rate[1];
                    vertex_color[3].a *= 1.0f - rate[1];
                }
                //  時間経過によるフェード
                if (em_trail.state & em_trail_item_state_use_time_fade)
                {
                    float fade = calculate_trail_time_fade(pt_common.life_time, pt_common.life_time_max, em_trail.time_fade_rate);
                    vertex_color[0].a *= fade;
                    vertex_color[1].a *= fade;
                    vertex_color[2].a *= fade;
                    vertex_color[3].a *= fade;
                }
                vertex_texcoord[0] = uv_rect.xy + uv_rect.zw * float2(0, rate[0]);
                vertex_texcoord[1] = uv_rect.xy + uv_rect.zw * float2(1, rate[0]);
                vertex_texcoord[2] = uv_rect.xy + uv_rect.zw * float2(0, rate[1]);
                vertex_texcoord[3] = uv_rect.xy + uv_rect.zw * float2(1, rate[1]);
            }

            //  頂点座標
            uint vertex_buffer_byte_offset = ((particle_header_offset * em_trail.max + index) * index_size) * vertex_size;
            register_vertex(target_vertex_buffer_index, vertex_buffer_byte_offset, vertex_positions, vertex_color, vertex_texcoord);
        }

        //  残りは初期化しておく
        vertex_positions[0] = vertex_positions[2];
        vertex_positions[1] = vertex_positions[3];
        vertex_color[0] = vertex_color[2];
        vertex_color[1] = vertex_color[3];
        vertex_texcoord[0] = vertex_texcoord[2];
        vertex_texcoord[1] = vertex_texcoord[3];
        [loop] for (index = pt_trail_header.counter; index < em_trail.max; ++index)
        {
            //  頂点座標
            uint vertex_buffer_byte_offset = ((particle_header_offset * em_trail.max + index) * index_size) * vertex_size;
            register_vertex(target_vertex_buffer_index, vertex_buffer_byte_offset, vertex_positions, vertex_color, vertex_texcoord);

            //  次の軌跡の情報取得
            read_offset = particle_binary_offset + (index + 0) * pt_trail_body_size_of_bytes;
            stream_trail_body(rw_read_only, read_offset, trail_body[0]);
            read_offset = particle_binary_offset + (index + 1) * pt_trail_body_size_of_bytes;
            stream_trail_body(rw_read_only, read_offset, trail_body[1]);
        }
    }
    else
    {
        //  スプライトの形状生成
        {
    	    //  ワールド行列生成
            float4x4 scale_matrix = matrix_scaling(pt_common.scale);
            float4x4 rotation_matrix = matrix_rotation_quaternion(pt_common.rotation);
            if (!has_flag(em_common.emitter_state, em_common_item_state_billboard_off))
            {
                //  ビルボード処理
                float4x4 billboard_matrix = inverse_view_transform;
                billboard_matrix._41_42_43_44 = float4(0, 0, 0, 1);
                rotation_matrix = billboard_matrix;
            }
            float4x4 translation_matrix = matrix_translation(pt_common.translation);
            float4x4 world_matrix = mul(mul(scale_matrix, rotation_matrix), translation_matrix);

            //  スプライトは4頂点
            vertex_positions[0] = mul(float4(-0.5f, -0.5f, 0, 1), world_matrix);
            vertex_positions[1] = mul(float4(+0.5f, -0.5f, 0, 1), world_matrix);
            vertex_positions[2] = mul(float4(-0.5f, +0.5f, 0, 1), world_matrix);
            vertex_positions[3] = mul(float4(+0.5f, +0.5f, 0, 1), world_matrix);
            vertex_color[0] = pt_common.color;
            vertex_color[1] = pt_common.color;
            vertex_color[2] = pt_common.color;
            vertex_color[3] = pt_common.color;
            float4 uv_rect = float4(0, 0, 1, 1);
            if (em_common.active_items & emitter_active_uv_item_bit_flag)
            {
                uv_rect = calculate_uv_rect(pt_common, em_uv);
            }
            vertex_texcoord[0] = uv_rect.xy + uv_rect.zw * float2(0, 0);
            vertex_texcoord[1] = uv_rect.xy + uv_rect.zw * float2(1, 0);
            vertex_texcoord[2] = uv_rect.xy + uv_rect.zw * float2(0, 1);
            vertex_texcoord[3] = uv_rect.xy + uv_rect.zw * float2(1, 1);
        }

        //  頂点座標
        uint vertex_buffer_byte_offset = (particle_header_offset * index_size) * vertex_size;
        register_vertex(target_vertex_buffer_index, vertex_buffer_byte_offset, vertex_positions, vertex_color, vertex_texcoord);
    }
}