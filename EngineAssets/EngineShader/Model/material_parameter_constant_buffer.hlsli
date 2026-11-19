#ifndef __MATERIAL_PARAMETER_CONSTANT_BUFFER_HLSLI__
#define __MATERIAL_PARAMETER_CONSTANT_BUFFER_HLSLI__

#include "common.hlsli"
#include "scene_constant_buffer.hlsli"

static const int shader_resource_view_max = 128;

static const uint srv_flag_srgb = 1 << 0;               //  sRGB色空間シェーダーリソースビュー
static const uint srv_flag_gamut_rec709 = 1 << 2;       //  テクスチャの色域 Rec.709
static const uint srv_flag_gamut_rec2020 = 1 << 3;      //  テクスチャの色域 Rec.2020

static const uint material_blend_mode_opaque = 0;		//	不透明描画
static const uint material_blend_mode_mask = 1;			//	不透明描画+透過マスク
static const uint material_blend_mode_translucent = 2;  //	半透明描画
static const uint material_blend_mode_addative = 3;     //  加算合成描画

cbuffer PARAMETER_CONSTANT_BUFFER : register(b2)
{
    int4 shader_resource_view_index[shader_resource_view_max / 4]; // SRV番号
    uint4 shader_resource_view_flags[shader_resource_view_max / 4]; // SRVフラグ類

    uint material_flags; // マテリアルフラグ類
    float material_alpha_clip_threshold; // 透明/不透明閾値
    uint material_shading_model; // シェーディングモデル
    uint material_blend_mode; // ブレンドモード
};

//  テクスチャ使用可否
bool has_resource(uint resource_id)
{
    return shader_resource_view_index[resource_id / 4][resource_id % 4] > -1;
}

bool is_convert_srgb(uint resource_id)
{
    return shader_resource_view_flags[resource_id / 4][resource_id % 4] & srv_flag_srgb;
}

//  テクスチャの色域から描画色域へ変換
float3 convert_texture_gamut_to_rcs(uint resource_id, float3 color)
{
    uint flags = shader_resource_view_flags[resource_id / 4][resource_id % 4];
    if (flags & srv_flag_gamut_rec709)
        return convert_linear_rec709_to_rcs(color);
    if (flags & srv_flag_gamut_rec2020)
        return convert_linear_rec2020_to_rcs(color);
    return color;
}

float4 TextureSample(Texture1D srv, uint resource_id, SamplerState samp, float texcoord)
{
    float4 color = srv.Sample(samp, texcoord);
    if (is_convert_srgb(resource_id))
        color.rgb = convert_srgb_to_linear(color.rgb);

    return color;
}

float4 Texture2dSample(Texture2D srv, uint resource_id, SamplerState samp, float2 texcoord)
{
    float4 color = srv.Sample(samp, texcoord);
    if (is_convert_srgb(resource_id))
        color.rgb = convert_srgb_to_linear(color.rgb);
    color.rgb = convert_texture_gamut_to_rcs(resource_id, color.rgb);

    return color;
}

float4 Texture2dSample(Texture3D srv, uint resource_id, SamplerState samp, float3 texcoord)
{
    float4 color = srv.Sample(samp, texcoord);
    if (is_convert_srgb(resource_id))
        color.rgb = convert_srgb_to_linear(color.rgb);
    color.rgb = convert_texture_gamut_to_rcs(resource_id, color.rgb);

    return color;
}

float4 TextureCubeSample(TextureCube srv, uint resource_id, SamplerState samp, float3 texcoord)
{
    float4 color = srv.Sample(samp, texcoord);
    if (is_convert_srgb(resource_id))
        color.rgb = convert_srgb_to_linear(color.rgb);
    color.rgb = convert_texture_gamut_to_rcs(resource_id, color.rgb);

    return color;
}

//  テクスチャサンプル
#define TEXTURE1D_SAMPLE(SRV, TEXCOORD)  Texture1dSample(SRV, SRV##Index, SRV##State, TEXCOORD)
#define TEXTURE2D_SAMPLE(SRV, TEXCOORD)  Texture2dSample(SRV, SRV##Index, SRV##State, TEXCOORD)
#define TEXTURE3D_SAMPLE(SRV, TEXCOORD)  Texture3dSample(SRV, SRV##Index, SRV##State, TEXCOORD)
#define TEXTURECUBE_SAMPLE(SRV, TEXCOORD)  TextureCubeSample(SRV, SRV##Index, SRV##State, TEXCOORD)

#define TEXTURE1D_SAMPLE_WITH_SAMPLER(SRV, SAMPLER, TEXCOORD)  Texture1dSample(SRV, SRV##Index, SAMPLER, TEXCOORD)
#define TEXTURE2D_SAMPLE_WITH_SAMPLER(SRV, SAMPLER, TEXCOORD)  Texture2dSample(SRV, SRV##Index, SAMPLER, TEXCOORD)
#define TEXTURE3D_SAMPLE_WITH_SAMPLER(SRV, SAMPLER, TEXCOORD)  Texture3dSample(SRV, SRV##Index, SAMPLER, TEXCOORD)
#define TEXTURECUBE_SAMPLE_WITH_SAMPLER(SRV, SAMPLER, TEXCOORD)  TextureCubeSample(SRV, SRV##Index, SAMPLER, TEXCOORD)

//  シェーダーリソースビューの名前構築
#define GET_SHADER_RESOURCE_VIEW_INDEX(NAME) NAME##Index

//  シェーダーリソース所持判定
#define HAS_RESOURCE(SRV) has_resource(GET_SHADER_RESOURCE_VIEW_INDEX(SRV))

//  シェーダーリソース登録用マクロ
#define REGISTER_SHADER_RESOURCE_VIEW(TYPE, NAME, INDEX) \
static const uint GET_SHADER_RESOURCE_VIEW_INDEX(NAME) = INDEX; \
TYPE NAME : register(t##INDEX); \
SamplerState GET_SAMPLER_STATE_INDEX(NAME);

//  サンプラーステート名前構築
#define GET_SAMPLER_STATE_INDEX(NAME) NAME##State

//  テクスチャ登録用マクロ
#define REGISTER_TEXTURE_DATA(SRV_TYPE, SAMPLER_TYPE, NAME, INDEX) \
static const uint GET_SHADER_RESOURCE_VIEW_INDEX(NAME) = INDEX; \
SRV_TYPE NAME : register(t##INDEX); \
SAMPLER_TYPE GET_SAMPLER_STATE_INDEX(NAME) : register(s##INDEX);

#endif  //  __MATERIAL_PARAMETER_CONSTANT_BUFFER_HLSLI__
