#pragma once

#include <cstdint>

//	色空間
namespace color_space
{
	//	色域
	enum class gamuts : std::uint8_t
	{
		rec709,		//	Rec.709
		rec2020,	//	Rec.2020
	};

	//	作業色空間
	constexpr gamuts rendering_gamut = gamuts::rec2020;

	//	sRGB 曲線(IEC 61966-2-1)
	float convert_srgb_to_linear(float value);
	float convert_linear_to_srgb(float value);
	math::vector3f convert_srgb_to_linear(const math::vector3f& value);
	math::vector3f convert_linear_to_srgb(const math::vector3f& value);

	//	色域変換
	math::float3x3 get_gamut_matrix(gamuts from, gamuts to);
	math::vector3f convert_gamut(const math::vector3f& value, gamuts from, gamuts to);

	//	CIE XYZ変換
	math::vector3f convert_xyz_to_gamut(const math::vector3f& xyz, gamuts to);
	math::vector3f convert_gamut_to_xyz(const math::vector3f& value, gamuts from);

	//	輝度変換
	math::vector3f get_luminance_coefficients(gamuts gamut);
	float get_luminance(const math::vector3f& value, gamuts gamut);

	//	3x3 行列を掛ける
	math::vector3f transform(const math::float3x3& matrix, const math::vector3f& value);
}
