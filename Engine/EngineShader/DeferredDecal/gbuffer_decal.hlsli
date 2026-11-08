//  下記の定数を定義してインクルードする事
//  #define HAS_NORMAL_MAP  : 法線マップを所持しているか？
#ifndef __GBUFFER_DECAL_HLSLI__
#define __GBUFFER_DECAL_HLSLI__

#include "scene_constant_buffer.hlsli"
#include "gbuffer.hlsli"
#include "material_parameter_constant_buffer.hlsli"

cbuffer DECAL_PRIMITIVE_CONSTANT_BUFFER
{
    row_major float4x4 decal_transform;
    row_major float4x4 decal_inverse_transform;
    float4 decal_direction;
};

static const float normal_angle = 0.45f;

////////////////////////////////////////////////////////////////////////////////////////////////////
struct VS_OUT
{
    float4 position : SV_POSITION;
};

struct PSDecalOut
{
    float4 base_color : SV_TARGET0;
    float4 normal_depth : SV_TARGET1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  デカール計算結果格納情報
struct DecalIn
{
    float3 w_position;
    float3 w_normal;
#if defined(HAS_NORMAL_MAP)
    float3 w_tangent;
    float3 w_binormal;
#endif  //  defined(HAS_NORMAL_MAP)
    float linear_depth;
    float2 texcoord;
};

struct DecalData
{
    float4 base_color;
    float3 w_normal;
    float linear_depth;
};

DecalIn convert_decal_in(float4 normal_depth, float2 texcoord)
{
    DecalIn din;
    din.w_normal = normalize(normal_depth.rgb);
    din.linear_depth = normal_depth.a;

    //  法線と向きから適応させるかどうかを決める
    clip(dot(decal_direction.xyz, din.w_normal) >= normal_angle ? -1 : 1);

    //  ワールド座標生成
    float depth = convert_linear_depth_to_projection_depth(din.linear_depth, z_buffer_parameteres);
    float4 w_position = float4(texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), depth, 1.0f);
    w_position = mul(w_position, inverse_view_projection_transform);
    w_position /= w_position.w;
    din.w_position = w_position.xyz;
    
    //  キューブ基準の空間の座標に変換してデカールのUV座標に変換
    float4 cube_texture_position = mul(w_position, decal_inverse_transform);
    cube_texture_position /= cube_texture_position.w;

    //  キューブの範囲外はクリップして描画対象外としておく
    clip(all(abs(cube_texture_position.xyz) <= 0.5f) ? 1 : -1);

    cube_texture_position.xy = cube_texture_position.xy + float2(0.5f, 0.5f);
    din.texcoord = cube_texture_position.xy;

#if defined(HAS_NORMAL_MAP)
    //  接線/従法線を生成
    float3 decal_tangent_ws = normalize(mul(float3(1.0f, 0.0f, 0.0f), (float3x3) decal_transform));
    float3 decal_binormal_ws = normalize(mul(float3(0.0f, 1.0f, 0.0f), (float3x3) decal_transform));

    float handedness = sign(determinant((float3x3) decal_transform));
    
    float ndotT = dot(decal_tangent_ws, din.w_normal);
    if (ndotT > 0.999f)
    {
        // X軸が法線と平行ならY軸を基準に
        din.w_binormal = normalize(decal_binormal_ws - din.w_normal * dot(decal_binormal_ws, din.w_normal));
        din.w_tangent = normalize(cross(din.w_binormal, din.w_normal) * handedness);
    }
    else
    {
        din.w_tangent = normalize(decal_tangent_ws - din.w_normal * dot(decal_tangent_ws, din.w_normal));
        din.w_binormal = normalize(cross(din.w_normal, din.w_tangent) * handedness);
    }
#endif  //  defined(HAS_NORMAL_MAP)
    
    return din;
}

PSDecalOut convert_decal_out(in DecalData decal_data)
{
    //  GBufferDataに出力情報をまとめる
    PSDecalOut pout = (PSDecalOut) 0;
    pout.base_color.rgb = decal_data.base_color.rgb;
    pout.base_color.a = material_shading_model;
    pout.normal_depth.xyz = normalize(decal_data.w_normal.xyz);
    pout.normal_depth.a = decal_data.linear_depth;
    return pout;
}
#endif // __GBUFFER_DECAL_HLSLI__
