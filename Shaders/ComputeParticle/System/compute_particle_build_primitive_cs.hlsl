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
float4 calculate_uv_rect(in pt_common_item pt_common, in em_sprite_animation_item em_sprite_animation)
{
    float u = 1.0f / (float) max(em_sprite_animation.split_count.x, 1);
    float v = 1.0f / (float) max(em_sprite_animation.split_count.y, 1);
    //  アニメーション制御
    int ui = 0, vi = 0;
    if (em_sprite_animation.increment_time > 0)
    {
        int uv_index = (pt_common.life_time_max - pt_common.life_time) / em_sprite_animation.increment_time;
        ui = uv_index % em_sprite_animation.split_count.x;
        vi = uv_index / em_sprite_animation.split_count.y;
    }
    float4 rect = float4(u * (ui + 0), v * (vi + 0), u * (ui + 1), v * (vi + 1));
    rect.z -= rect.x;
    rect.w -= rect.y;
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
    em_sprite_animation_item em_sprite_animation;
    {
        stream_common_item(rw_read_only, emitter_binary_offset, em_common);
        if (em_common.active_items & emitter_active_trail_item_bit_flag)
        {
            stream_trail_item(rw_read_only, emitter_binary_offset, em_trail);
        }
        if (em_common.active_items & emitter_active_sprite_animation_item_bit_flag)
        {
            stream_sprite_animation_item(rw_read_only, emitter_binary_offset, em_sprite_animation);
        }
        else
        {
            em_sprite_animation.split_count = uint2(1, 1); //	分割数
            em_sprite_animation.increment_time = -1; //	次のスプライトに移動するタイミング
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
        //  pt_sprite_animation_item は存在しない

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
            read_offset = particle_binary_offset;
            stream_trail_body(rw_read_only, read_offset, trail_body[1]);

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
        float4 uv_rect = calculate_uv_rect(pt_common, em_sprite_animation);
        
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
            float4 uv_rect = calculate_uv_rect(pt_common, em_sprite_animation);
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