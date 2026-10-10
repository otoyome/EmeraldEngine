#pragma once

//	ライトの種別
enum class light_type
{
	directional,	//  平行光源
	point,			//  点光源
	spot,			//  スポットライト

	light_probe,	//	ライトプローブ
};

//	attribute のビット構成
//	0～7byte : ライトフラグ
//	8～19byte : 影番号
//	20～21byte : デバッグフラグ
//	24～27byte : ライト種別
constexpr UINT light_attribute_flags_mask = 0x003000ffu;		//	ライトフラグとデバッグフラグ
constexpr UINT light_attribute_shadow_index_shift = 8;
constexpr UINT light_attribute_shadow_index_mask = 0xfffu;
constexpr UINT light_attribute_invalid_shadow_index = 0xfffu;	//	影無し
constexpr UINT light_attribute_type_shift = 24;
constexpr UINT light_attribute_type_mask = 0xfu;

struct directional_lights
{
	math::float3 direction{ 1.0f, -1.0f, -1.0f };	//	向き
	float dummy{ 0 };
	math::float3 color{ 1, 1, 1 };					//	色 × 強さ
	UINT attribute{ 0 };							//	ライトフラグ
};
static_assert(sizeof(directional_lights) == 32, "directional_lights must be 32 bytes");

struct point_lights
{
	math::float3 position{ 0, 0, 0 };	//	座標
	float inv_range{ 0 };				//	1 / 影響範囲
	math::float3 color{ 1, 1, 1 };		//	色 × 強さ
	UINT attribute{ 0 };				//	ライトフラグ
};
static_assert(sizeof(point_lights) == 32, "point_lights must be 32 bytes");

struct spot_lights
{
	math::float3 position{ 0, 0, 0 };	//	座標
	float inv_range{ 0 };				//	1 / 影響範囲
	math::float3 color{ 1, 1, 1 };		//	色 × 強さ
	UINT attribute{ 0 };				//	ライトフラグ
	math::float3 direction{ 0, 0, 1 };	//	向き
	UINT cone{ 0 };						//	16byte : インナーcos, 16byte : アウターcos
};
static_assert(sizeof(spot_lights) == 48, "spot_lights must be 48 bytes");

struct light_probes
{
	math::float4 position{ 0, 0, 0, 0 };
	math::float4 color{ 1, 1, 1, 1 };
	math::float4 inner_area{ 100, 100, 100, 0 };
	math::float4 outer_area{ 200, 200, 200, 0 };
};

struct light_data
{
	//  共有の情報
	//	work_data[1].w は共有で attribute を入れておく
	math::float4 work_data[3];
};
static_assert(sizeof(light_data) == 48, "light_data must be 48 bytes");

namespace light_data_utility
{
	directional_lights convert_directional_lights(const light_data& data);

	point_lights convert_point_lights(const light_data& data);

	spot_lights convert_spot_lights(const light_data& data);

	light_data convert_light_data(const directional_lights& light);

	light_data convert_light_data(const point_lights& light);

	light_data convert_light_data(const spot_lights& light);

	light_type get_light_type(const light_data& data);

	UINT make_light_attribute(UINT flags, light_type type, UINT shadow_index = light_attribute_invalid_shadow_index);

	light_type get_light_type(UINT attribute);

	UINT get_shadow_index(UINT attribute);

	math::float3 get_position(const light_data& data);

	math::float3 get_direction(const light_data& data);

	math::float4 get_color(const light_data& data);

	float convert_inv_range(float range);

	UINT encode_spot_cone(float inner_cone, float outer_cone);

	math::float2 decode_spot_cone(UINT cone);

}	//	namespace light_data_utility
