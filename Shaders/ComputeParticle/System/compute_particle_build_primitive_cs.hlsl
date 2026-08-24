#include "compute_particle.hlsli"

float repeat(float _max, float _value)
{
    float t = fmod(_value, _max);
    return t < 0.0f ? _max + t : t;
}
 
float2 repeat(float2 _max, float2 _value)
{
    float2 t = fmod(_value, _max);
    return t < 0.0f ? _max + t : t; // componentwise ternary (SM4+)
}
 
float3 repeat(float3 _max, float3 _value)
{
    float3 t = fmod(_value, _max);
    return t < 0.0f ? _max + t : t;
}
 
float4 repeat(float4 _max, float4 _value)
{
    float4 t = fmod(_value, _max);
    return t < 0.0f ? _max + t : t;
}

float wrap(float _min, float _max, float _value)
{
    return repeat(_max - _min, _value - _min) + _min;
}
 
float2 wrap(float2 _min, float2 _max, float2 _value)
{
    return repeat(_max - _min, _value - _min) + _min;
}
 
float3 wrap(float3 _min, float3 _max, float3 _value)
{
    return repeat(_max - _min, _value - _min) + _min;
}
 
float4 wrap(float4 _min, float4 _max, float4 _value)
{
    return repeat(_max - _min, _value - _min) + _min;
}

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

//  ShadeModel5.0ではこうしないとできないので……辛み
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
            rect.x += speed;
            rect.y += speed;
            rect.z += speed;
            rect.w += speed;
        }
    }
    else
    {
        uint sx = max(em_uv.split_count.x, 1);
        uint sy = max(em_uv.split_count.y, 1);

        //  UV矩形設定
        float w = rect.z - rect.x;
        float h = rect.w - rect.y;
        float u = w / sx;
        float v = h / sy;

        //  番号指定
        int uv_index = em_uv.index;

        //  アニメーション制御
        if (em_uv.state & emitter_uv_item_state_animation)
        {
            uv_index += (sx * sy) * (pt_common.life_time_max - pt_common.life_time) / em_uv.time;
        }
        uv_index = repeat(sx * sy, uv_index);

        rect.x += u * (uv_index % sx);
        rect.y += v * (uv_index / sx);
        rect.z = rect.x + u;
        rect.w = rect.y + v;
    }
    return rect;
}

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
    em_common_item em_common;
    em_trail_item em_trail;
    em_uv_item em_uv;
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
        else
        {
            em_uv.state = 0;
            em_uv.split_count = uint2(1, 1); //	分割数
            em_uv.index = 0;
            em_uv.time = -1; //	次のスプライトに移動するタイミング
        }

        //  以降は不要
    }

    //  パーティクル共通情報取得
    pt_common_item pt_common;
    pt_trail_header pt_trail_header;
    {
        stream_common_item(rw_read_only, particle_binary_offset, pt_common);
        if (em_common.active_items & emitter_active_trail_item_bit_flag)
        {
            stream_trail_header(rw_read_only, particle_binary_offset, pt_trail_header);
        }
        //  pt_uv_item は存在しない

        //  以降は不要
    }

    //  軌跡形状アイテムを取得しているか判定
    float4 vertex_color[4], vertex_positions[4];
    float2 vertex_texcoord[4];
    if (em_common.active_items & emitter_active_trail_item_bit_flag)
    {
        //  プリミティブ生成
        uint read_offset = 0;

        //  初期値設定
        pt_trail_body trail_body[2];
        {
            trail_body[0].scale = pt_common.scale;
            trail_body[0].rotation = pt_common.rotation;
            trail_body[0].translation = pt_common.translation;
            if (pt_trail_header.counter >= 2)
            {
                read_offset = particle_binary_offset + pt_trail_body_size_of_bytes;
                stream_trail_body(rw_read_only, read_offset, trail_body[1]);
            }
            else
            {
                trail_body[1] = trail_body[0];
            }

            float3 x_axis = float3(1, 0, 0);
            float3 z_axis = trail_body[1].translation - trail_body[0].translation;
            if (length(z_axis) > 0.001f)
            {
                x_axis = cross(normalize(z_axis), normalize(-camera_direction.xyz));
            }
            vertex_positions[2] = float4(trail_body[0].translation + x_axis * trail_body[0].scale, 1);
            vertex_positions[3] = float4(trail_body[0].translation - x_axis * trail_body[0].scale, 1);
        }
        
        //  UV矩形
        float4 uv_rect = calculate_uv_rect(pt_common, em_uv);
        
        //  頂点登録
        uint index = 0;
        [loop] for (index = 0; index < pt_trail_header.counter; ++index)
        {
            vertex_positions[0] = vertex_positions[2];
            vertex_positions[1] = vertex_positions[3];

            //  座標計算
            float3 x_axis = float3(1, 0, 0);
            float3 z_axis = trail_body[1].translation - trail_body[0].translation;
            if (length(z_axis) > 0.001f)
            {
                x_axis = cross(normalize(z_axis), normalize(-camera_direction.xyz));
            }
            vertex_positions[2] = float4(trail_body[0].translation + x_axis * trail_body[0].scale, 1);
            vertex_positions[3] = float4(trail_body[0].translation - x_axis * trail_body[0].scale, 1);
            vertex_positions[2] = (vertex_positions[0] + vertex_positions[2]) * 0.5f;
            vertex_positions[3] = (vertex_positions[1] + vertex_positions[3]) * 0.5f;

            //  スプライトの形状生成
            {
                //  スプライトは4頂点
                float rate[2] = { saturate((float) (index + 0) / em_trail.max), saturate((float) (index + 1) / em_trail.max) };
                vertex_color[0] = float4(1, 1, 1, rate[0]);
                vertex_color[1] = float4(1, 1, 1, rate[0]);
                vertex_color[2] = float4(1, 1, 1, rate[1]);
                vertex_color[3] = float4(1, 1, 1, rate[1]);
                vertex_texcoord[0] = uv_rect.xy + uv_rect.zw * float2(0, rate[0]);
                vertex_texcoord[1] = uv_rect.xy + uv_rect.zw * float2(1, rate[0]);
                vertex_texcoord[2] = uv_rect.xy + uv_rect.zw * float2(0, rate[1]);
                vertex_texcoord[3] = uv_rect.xy + uv_rect.zw * float2(1, rate[1]);
            }

            //  頂点座標
            uint vertex_buffer_byte_offset = ((particle_header_offset * em_trail.max + index) * index_size) * vertex_size;
            register_vertex(target_vertex_buffer_index, vertex_buffer_byte_offset, vertex_positions, vertex_color, vertex_texcoord);

            //  次の軌跡の情報取得
            read_offset = particle_binary_offset + (index + 0) * pt_trail_body_size_of_bytes;
            stream_trail_body(rw_read_only, read_offset, trail_body[0]);
            read_offset = particle_binary_offset + (index + 1) * pt_trail_body_size_of_bytes;
            stream_trail_body(rw_read_only, read_offset, trail_body[1]);
        }

        vertex_positions[0] = float4(0, 0, 0, 1);
        vertex_positions[1] = float4(0, 0, 0, 1);
        vertex_positions[2] = float4(0, 0, 0, 1);
        vertex_positions[3] = float4(0, 0, 0, 1);
        vertex_color[0] = float4(0, 0, 0, 0);
        vertex_color[1] = float4(0, 0, 0, 0);
        vertex_color[2] = float4(0, 0, 0, 0);
        vertex_color[3] = float4(0, 0, 0, 0);
        vertex_texcoord[0] = float2(0, 0);
        vertex_texcoord[1] = float2(0, 0);
        vertex_texcoord[2] = float2(0, 0);
        vertex_texcoord[3] = float2(0, 0);
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
            float4x4 billboard_matrix = inverse_view_transform;
            billboard_matrix._41_42_43_44 = float4(0, 0, 0, 1);
            float4x4 rotation_matrix = mul(billboard_matrix, matrix_rotation_quaternion(pt_common.rotation));
            float4x4 translation_matrix = matrix_translation(pt_common.translation);
            float4x4 world_matrix = mul(mul(scale_matrix, rotation_matrix), translation_matrix);

            //  スプライトは4頂点
            vertex_positions[0] = mul(float4(-0.5f, -0.5f, 0, 1), world_matrix);
            vertex_positions[1] = mul(float4(+0.5f, -0.5f, 0, 1), world_matrix);
            vertex_positions[2] = mul(float4(-0.5f, +0.5f, 0, 1), world_matrix);
            vertex_positions[3] = mul(float4(+0.5f, +0.5f, 0, 1), world_matrix);
            vertex_color[0] = float4(1, 1, 1, 1);
            vertex_color[1] = float4(1, 1, 1, 1);
            vertex_color[2] = float4(1, 1, 1, 1);
            vertex_color[3] = float4(1, 1, 1, 1);
            float4 uv_rect = calculate_uv_rect(pt_common, em_uv);
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