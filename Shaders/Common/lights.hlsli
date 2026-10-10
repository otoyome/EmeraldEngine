#ifndef __LIGHTS_HLSLI__
#define __LIGHTS_HLSLI__

// 平行光源
struct directional_lights
{
    float4 direction;
    float4 color;
};

// 点光源
struct point_lights
{
	float3	position;	//	座標
	float	inv_range;	//	1 / 影響範囲
	float3	color;		//	色 × 強さ
	uint	attribute;	//	ライトフラグ
};

// スポットライト
struct spot_lights
{
	float3	position;	//	座標
	float	inv_range;	//	1 / 影響範囲
	float3	color;		//	色 × 強さ
	uint	attribute;	//	ライトフラグ
	float3	direction;	//	向き
	uint	cone;		//	16byte : インナーcos, 16byte : アウターcos
};

//	距離減衰
float calc_light_distance_attenuation(float distance, float inv_range)
{
	float attenuate_length = saturate(1.0f - distance * inv_range);
	return attenuate_length * attenuate_length;
}

//	角度取り出し関数
float2 decode_spot_cone(uint cone)
{
	int inner = ((int) (cone << 16)) >> 16;
	int outer = ((int) cone) >> 16;
	return max(float2(inner, outer) / 32767.0f, -1.0f);
}

//	スポットライトの角度減衰
float calc_spot_cone_attenuation(spot_lights light, float3 L)
{
	float2 cone = decode_spot_cone(light.cone);
	float area = cone.x - cone.y;
	return saturate(1.0f - (cone.x - dot(light.direction, L)) / area);
}

// ライトプローブ
struct light_probes
{
	float4 position;
	float4 color;
	float4 inner_area;
	float4 outer_area;
};

//  ライト種別ID
static const int light_kind_directional_light = 1; //  平行光源
static const int light_kind_point_light = 2; //  点光源
static const int light_kind_spot_light = 3; //  スポットライト

//  ライト系のマスク値
static const uint light_flag_active = 1 << 0;
static const uint light_flag_use_shadow = 1 << 1;

static const uint light_flag_debug_csm_area_flag = 1 << 20;
static const uint light_flag_debug_visualize_ailiasing_error_flag = 1 << 21;

//  纏めたライト情報
struct compund_lights
{
    //  共有の情報
    //  work_data[3].w は共有でライトの種別番号を入れておく
    float4 work_data[4];
};

#endif	//	__LIGHTS_HLSLI__
