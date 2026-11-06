#include "Graphics/color_space.h"

#include <algorithm>
#include <cmath>

namespace
{
	//	行優先の 3x3 行列を作る
	math::float3x3 make_matrix(float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22)
	{
		math::float3x3 result;
		result(0, 0) = m00; result(0, 1) = m01; result(0, 2) = m02;
		result(1, 0) = m10; result(1, 1) = m11; result(1, 2) = m12;
		result(2, 0) = m20; result(2, 1) = m21; result(2, 2) = m22;
		return result;
	}

	//	Rec.709 リニア <> Rec.2020 リニア
	const math::float3x3 rec709_to_rec2020 = make_matrix(
		0.627402f, 0.329292f, 0.043306f,
		0.069095f, 0.919544f, 0.011360f,
		0.016394f, 0.088028f, 0.895578f);
	const math::float3x3 rec2020_to_rec709 = make_matrix(
		+1.660496f, -0.587656f, -0.072840f,
		-0.124547f, +1.132895f, -0.008348f,
		-0.018154f, -0.100597f, +1.118751f);

	//	Rec.709 リニア <> XYZ
	const math::float3x3 rec709_to_xyz = make_matrix(
		0.41239080f, 0.35758434f, 0.18048079f,
		0.21263901f, 0.71516868f, 0.07219232f,
		0.01933082f, 0.11919478f, 0.95053215f);
	const math::float3x3 xyz_to_rec709 = make_matrix(
		+3.24096994f, -1.53738318f, -0.49861076f,
		-0.96924364f, +1.87596750f, +0.04155506f,
		+0.05563008f, -0.20397696f, +1.05697151f);

	//	Rec.2020 リニア <> XYZ
	const math::float3x3 rec2020_to_xyz = make_matrix(
		+0.63695806f, +0.14461690f, +0.16888096f,
		+0.26270020f, +0.67799806f, +0.05930171f,
		+0.00000000f, +0.02807269f, +1.06098508f);
	const math::float3x3 xyz_to_rec2020 = make_matrix(
		+1.71665119f, -0.35567078f, -0.25336628f,
		-0.66668435f, +1.61648124f, +0.01576855f,
		+0.01763986f, -0.04277061f, +0.94210312f);
}

float color_space::convert_srgb_to_linear(float value)
{
	value = (std::max)(value, 0.0f);
	return value < 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

float color_space::convert_linear_to_srgb(float value)
{
	value = (std::max)(value, 0.0f);
	return value < 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

math::vector3f color_space::convert_srgb_to_linear(const math::vector3f& value)
{
	return math::vector3f(convert_srgb_to_linear(value[0]), convert_srgb_to_linear(value[1]), convert_srgb_to_linear(value[2]));
}

math::vector3f color_space::convert_linear_to_srgb(const math::vector3f& value)
{
	return math::vector3f(convert_linear_to_srgb(value[0]), convert_linear_to_srgb(value[1]), convert_linear_to_srgb(value[2]));
}

math::float3x3 color_space::get_gamut_matrix(gamuts from, gamuts to)
{
	if (from == gamuts::rec709 && to == gamuts::rec2020)
		return rec709_to_rec2020;
	if (from == gamuts::rec2020 && to == gamuts::rec709)
		return rec2020_to_rec709;
	return math::float3x3();
}

math::vector3f color_space::convert_gamut(const math::vector3f& value, gamuts from, gamuts to)
{
	return from == to ? value : transform(get_gamut_matrix(from, to), value);
}

math::vector3f color_space::convert_xyz_to_gamut(const math::vector3f& xyz, gamuts to)
{
	return transform(to == gamuts::rec2020 ? xyz_to_rec2020 : xyz_to_rec709, xyz);
}

math::vector3f color_space::convert_gamut_to_xyz(const math::vector3f& value, gamuts from)
{
	return transform(from == gamuts::rec2020 ? rec2020_to_xyz : rec709_to_xyz, value);
}

math::vector3f color_space::get_luminance_coefficients(gamuts gamut)
{
	return gamut == gamuts::rec2020 ? math::vector3f(0.2627f, 0.6780f, 0.0593f) : math::vector3f(0.2126f, 0.7152f, 0.0722f);
}

float color_space::get_luminance(const math::vector3f& value, gamuts gamut)
{
	return math::vector3f::dot(value, get_luminance_coefficients(gamut));
}

math::vector3f color_space::transform(const math::float3x3& matrix, const math::vector3f& value)
{
	return math::vector3f(
		matrix.m[0][0] * value[0] + matrix.m[0][1] * value[1] + matrix.m[0][2] * value[2],
		matrix.m[1][0] * value[0] + matrix.m[1][1] * value[1] + matrix.m[1][2] * value[2],
		matrix.m[2][0] * value[0] + matrix.m[2][1] * value[1] + matrix.m[2][2] * value[2]);
}
