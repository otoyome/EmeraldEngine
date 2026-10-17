#ifndef __FORWARD_LIGHT_DATA_HLSLI__
#define __FORWARD_LIGHT_DATA_HLSLI__

#include "lights.hlsli"

//  フォワード用光源情報
static const uint directional_light_count = 4;
static const uint point_light_count = 8;
static const uint spot_light_count = 8;
cbuffer FORWARD_LIGHTS_CONSTANT_BUFFER : register(b3)
{
    uint4 forward_only_light_counts; //  x : 平行光源, y : 点光源, z : スポットライト, w : 空き
    light_data forward_only_directional_light_data[directional_light_count];
    light_data forward_only_point_light_data[point_light_count];
    light_data forward_only_spot_light_data[spot_light_count];
    light_probes forward_only_light_probe_data;
};

//  フォワード用IBLテクスチャ
//	@tool フレーム・視点層（シェーダーリソースは t96-t127、サンプラーは s14-s15）
TextureCube forward_only_diffuse_iem : register(t123);
TextureCube forward_only_specular_pmrem : register(t124);
Texture2D forward_only_lut_ggx : register(t125);
SamplerState forward_only_ibl_sampler_state : register(s14);

#endif  //  __FORWARD_LIGHT_DATA_HLSLI__
