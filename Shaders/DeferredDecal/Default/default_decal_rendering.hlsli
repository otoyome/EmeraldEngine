#ifndef __DEFAULT_DECAL_RENDERING_HLSLI__
#define __DEFAULT_DECAL_RENDERING_HLSLI__

#define HAS_NORMAL_MAP
#include "gbuffer_decal.hlsli"

REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseColorMap, 0)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, NormalMap, 1);

//  サンプラーステート
SamplerState sampler_states : register(s0);

//  マテリアル情報
cbuffer DEFAULT_MATERIAL_CONSTANT_BUFFER
{
    float4 base_color_factor;   //	ベースカラー
    float normal_map_scale;     //	法線マップのスケーリング値
};

#endif  //  __DEFAULT_DECAL_RENDERING_HLSLI__
