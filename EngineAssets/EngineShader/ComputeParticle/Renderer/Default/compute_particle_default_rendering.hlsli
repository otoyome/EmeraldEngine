#ifndef __COMPUTE_PARTICLES_DEFAULT_RENDERING_HLSLI__
#define __COMPUTE_PARTICLES_DEFAULT_RENDERING_HLSLI__

#include "compute_particle_renderer.hlsli"
#include "shadow_map.hlsli"    //  surface_forward_ps.hlsli のライティングで使用

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseColorMap, 0)

//  マテリアル情報
cbuffer DEFAULT_MATERIAL_CONSTANT_BUFFER
{
    float4  base_color_factor;          //  ベースカラー
    float   absolute_luminance_rate;    //  自己発光の絶対輝度の割合
};

float3 convert_particle_emission_to_radiance(float3 emission)
{
    return emission * lerp(exposure_white_radiance, 1.0f, saturate(absolute_luminance_rate));
}

#endif  //  __COMPUTE_PARTICLES_DEFAULT_RENDERING_HLSLI__
