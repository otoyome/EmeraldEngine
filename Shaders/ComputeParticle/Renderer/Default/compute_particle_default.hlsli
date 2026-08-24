#ifndef __COMPUTE_PARTICLES_DEFAULT_HLSLI__
#define __COMPUTE_PARTICLES_DEFAULT_HLSLI__

#include "compute_particle_renderer.hlsli"

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseColorMap, 0)

//  サンプラーステート
SamplerState linear_sampler_state : register(s0);

//  マテリアル情報
cbuffer DEFAULT_MATERIAL_CONSTANT_BUFFER
{
	float4  base_color_factor;		//	ベースカラー
};

#endif  //  __COMPUTE_PARTICLES_DEFAULT_HLSLI__
