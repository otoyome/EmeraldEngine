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

struct light_probes
{
	math::float4 position{ 0, 0, 0, 0 };
	math::float4 color{ 1, 1, 1, 1 };
	math::float4 inner_area{ 100, 100, 100, 0 };
	math::float4 outer_area{ 200, 200, 200, 0 };
};

//	ライト情報
//	平行光源は position / inv_range、点光源は direction / cone を使わない
struct light_data
{
	math::float3 position{ 0, 0, 0 };	//	座標
	float inv_range{ 0 };				//	1 / 影響範囲
	math::float3 color{ 1, 1, 1 };		//	色 × 強さ
	UINT attribute{ 0 };				//	ライトフラグ
	math::float3 direction{ 0, 0, 1 };	//	向き
	UINT cone{ 0 };						//	16byte : インナーcos, 16byte : アウターcos
};
static_assert(sizeof(light_data) == 48, "light_data must be 48 bytes");

namespace light_data_utility
{
	light_type get_light_type(const light_data& data);

	UINT make_light_attribute(UINT flags, light_type type, UINT shadow_index = light_attribute_invalid_shadow_index);

	light_type get_light_type(UINT attribute);

	UINT get_shadow_index(UINT attribute);

	float convert_inv_range(float range);

	UINT encode_spot_cone(float inner_cone, float outer_cone);

	math::float2 decode_spot_cone(UINT cone);

}	//	namespace light_data_utility
