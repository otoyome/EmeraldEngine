#include "Graphics/light_utility.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "Graphics/color_space.h"

namespace
{
	constexpr double pi = 3.14159265358979323846;
}

double light_utility::convert_lumen_to_candela(double lumen, double solid_angle)
{
	assert(solid_angle > 0.0);
	return lumen / solid_angle;
}

double light_utility::convert_candela_to_lumen(double candela, double solid_angle)
{
	return candela * solid_angle;
}

double light_utility::convert_lumen_to_lux(double lumen, double illuminated_area)
{
	assert(illuminated_area > 0.0);
	return lumen / illuminated_area;
}

double light_utility::convert_lux_to_lumen(double lux, double illuminated_area)
{
	return lux * illuminated_area;
}

double light_utility::convert_candela_to_lux(double candela, double distance)
{
	assert(distance > 0.0);
	return candela / (distance * distance);
}

double light_utility::convert_lux_to_candela(double lux, double distance)
{
	return lux * (distance * distance);
}

double light_utility::convert_candela_to_nit(double candela, double emitting_area)
{
	assert(emitting_area > 0.0);
	return candela / emitting_area;
}

double light_utility::convert_nit_to_candela(double nits, double emitting_area)
{
	return nits * emitting_area;
}

double light_utility::convert_point_light_lumen_to_candela(double lumen)
{
	return convert_lumen_to_candela(lumen, 4.0 * pi);
}

double light_utility::convert_point_light_candela_to_lumen(double candela)
{
	return convert_candela_to_lumen(candela, 4.0 * pi);
}

double light_utility::calculate_spot_light_solid_angle(double cos_half_angle)
{
	//	0 sr だと変換できないので下限を設ける
	const double solid_angle = 2.0 * pi * (1.0 - cos_half_angle);
	return (std::max)(solid_angle, 1e-6);
}

double light_utility::convert_spot_light_lumen_to_candela(double lumen, double cos_half_angle)
{
	return convert_lumen_to_candela(lumen, calculate_spot_light_solid_angle(cos_half_angle));
}

double light_utility::convert_spot_light_candela_to_lumen(double candela, double cos_half_angle)
{
	return convert_candela_to_lumen(candela, calculate_spot_light_solid_angle(cos_half_angle));
}

double light_utility::convert_photometric_to_radiometric(double photometric_value)
{
	return photometric_value / luminous_efficacy;
}

double light_utility::convert_radiometric_to_photometric(double radiometric_value)
{
	return radiometric_value * luminous_efficacy;
}

math::float2 light_utility::get_temperature_chromaticity(int kelvin)
{
	//	CIE 1960 uv で求めてから xy へ変換する(1000K ~ 15000K で有効)
	const double t = static_cast<double>((std::clamp)(kelvin, kelvin_minimum, kelvin_maximum));
	const double u = (0.860117757 + 1.54118254e-4 * t + 1.28641212e-7 * t * t) / (1.0 + 8.42420235e-4 * t + 7.08145163e-7 * t * t);
	const double v = (0.317398726 + 4.22806245e-5 * t + 4.20481691e-8 * t * t) / (1.0 - 2.89741816e-5 * t + 1.61456053e-7 * t * t);
	const double d = 2.0 * u - 8.0 * v + 4.0;
	math::float2 result;
	result[0] = static_cast<float>(3.0 * u / d);
	result[1] = static_cast<float>(2.0 * v / d);
	return result;
}

math::vector3f light_utility::get_temperature_color(int kelvin)
{
	//	輝度 Y = 1 の XYZ から Rec.2020 へ(1000K 付近はわずかに色域外になるので負の値を切り捨てて輝度を正規化し直す)
	const math::float2 xy = get_temperature_chromaticity(kelvin);
	const math::vector3f xyz(xy[0] / xy[1], 1.0f, (1.0f - xy[0] - xy[1]) / xy[1]);
	math::vector3f color = color_space::convert_xyz_to_gamut(xyz, color_space::gamuts::rec2020);
	color = math::vector3f((std::max)(color[0], 0.0f), (std::max)(color[1], 0.0f), (std::max)(color[2], 0.0f));
	return normalize_luminance_rec2020(color);
}

math::vector3f light_utility::normalize_luminance_rec2020(const math::vector3f& color)
{
	const float luminance = color_space::get_luminance(color, color_space::gamuts::rec2020);
	return luminance > 0.0f ? color * (1.0f / luminance) : color;
}
