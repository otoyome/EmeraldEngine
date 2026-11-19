#ifndef __COMMON_GBUFFER_LOADER_HLSLI__
#define __COMMON_GBUFFER_LOADER_HLSLI__

#include "gbuffer.hlsli"
#include "scene_constant_buffer.hlsli"

Texture2D<float4> gbuffer_base_color : register(t0);
Texture2D<float4> gbuffer_normal_tangent : register(t1);
Texture2D<float4> gbuffer_parameter : register(t2);
Texture2D<float2> gbuffer_velocity : register(t3);
Texture2D<float4> gbuffer_custom_data0 : register(t4);
Texture2D<float4> gbuffer_custom_data1 : register(t5);
Texture2D<float4> gbuffer_custom_data2 : register(t6);
Texture2D<float> gbuffer_depth : register(t7);

SamplerState sampler_states : register(s0);

GBufferTextures get_gbuffer_textures()
{
    GBufferTextures gbuffer_textures;
    gbuffer_textures.base_color = gbuffer_base_color;
    gbuffer_textures.normal_tangent = gbuffer_normal_tangent;
    gbuffer_textures.parameter = gbuffer_parameter;
    gbuffer_textures.velocity = gbuffer_velocity;
    gbuffer_textures.custom_data0 = gbuffer_custom_data0;
    gbuffer_textures.custom_data1 = gbuffer_custom_data1;
    gbuffer_textures.custom_data2 = gbuffer_custom_data2;
    gbuffer_textures.depth = gbuffer_depth;
    return gbuffer_textures;
}

GBufferData get_gbuffer_data(GBufferTextures gbuffer_textures, float2 texcoord)
{
    return DecodeGBuffer(gbuffer_textures, texcoord, inverse_view_projection_transform, z_buffer_parameteres);
}

GBufferData get_gbuffer_data(float2 texcoord)
{
    return get_gbuffer_data(get_gbuffer_textures(), texcoord);
}

#endif  //  __COMMON_GBUFFER_LOADER_HLSLI__