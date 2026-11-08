#ifndef __SURFACE_HLSLI__
#define __SURFACE_HLSLI__

//  サーフェイス計算結果と、それを GBufferData へ変換するヘルパー
#include "gbuffer.hlsli"

////////////////////////////////////////////////////////////////////////////////////////////////////
//  サーフェイス計算結果格納情報
struct SurfaceData
{
    float4 base_color; //  ベースカラー
    float3 emissive_color; //  自己発光
    uint shading_model; //  シェーディング方式
    float3 w_normal; //  ワールド法線
    float3 w_position; //  ワールド座標
    float2 velocity; //  速度
    float roughness; //  粗さ
    float metalness; //  金属質
    float occlusion_factor; //  遮蔽値
    float occlusion_strength; //  遮蔽強度
    uint stencil; //  ビットフラグ(4バイト=32フラグ)
    uint3 unique_data; //  シェーディングモデル毎に変わる変数(32ビット符号なし整数)
    float2 unique_parameter; //  シェーディングモデル毎に変わる変数(32ビット浮動小数点)

    //  [エンジン改造:G-Buffer]
    float3 shade_color; //  トゥーン用：陰色
    float4 ilm_color; //  トゥーン用：イルミネーション色
};

GBufferData convert_g_buffer_data(in SurfaceData surface_data)
{
    GBufferData g_buffer_data = (GBufferData)0;
    g_buffer_data.base_color = surface_data.base_color.rgb;
    g_buffer_data.shading_model = surface_data.shading_model;
    g_buffer_data.w_normal = surface_data.w_normal;
    g_buffer_data.w_position = surface_data.w_position;
    g_buffer_data.velocity = surface_data.velocity;
    g_buffer_data.roughness = surface_data.roughness;
    g_buffer_data.metalness = surface_data.metalness;
    g_buffer_data.occlusion_factor = surface_data.occlusion_factor;
    g_buffer_data.occlusion_strength = surface_data.occlusion_strength;
    g_buffer_data.stencil = surface_data.stencil;
    g_buffer_data.unique_parameter = surface_data.unique_parameter;
    g_buffer_data.unique_data = surface_data.unique_data;

    //  [エンジン改造:G-Buffer]
    g_buffer_data.shade_color = surface_data.shade_color;
    g_buffer_data.ilm_color = surface_data.ilm_color;
    //  [エンジン改造:G-Buffer]

    return g_buffer_data;
}

#endif  //  __SURFACE_HLSLI__
