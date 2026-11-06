#pragma once

//	ライト関数群
namespace light_utility
{
	//	最大視感効率 [lm/W](555nm)
	constexpr double luminous_efficacy = 683.0;

	//	色温度の範囲と太陽光の値
	constexpr int kelvin_minimum = 1000;
	constexpr int kelvin_maximum = 12000;
	constexpr int kelvin_sun = 5500;

	//	光度[cd] = 光束[lm] / 立体角[sr]
	double convert_lumen_to_candela(double lumen, double solid_angle);
	double convert_candela_to_lumen(double candela, double solid_angle);

	//	照度[lx] = 光束[lm] / 照射面積[m^2]
	double convert_lumen_to_lux(double lumen, double illuminated_area);
	double convert_lux_to_lumen(double lux, double illuminated_area);

	//	照度[lx] = 光度[cd] / 距離[m]^2(光源に正対する面での逆二乗則)
	double convert_candela_to_lux(double candela, double distance);
	double convert_lux_to_candela(double lux, double distance);

	//	輝度[nit] = 光度[cd] / 発光面の面積[m^2]
	double convert_candela_to_nit(double candela, double emitting_area);
	double convert_nit_to_candela(double nits, double emitting_area);

	//	点光源の光束と光度(全方向 4π sr)
	double convert_point_light_lumen_to_candela(double lumen);
	double convert_point_light_candela_to_lumen(double candela);

	//	スポットライトの光束と光度(cos_half_angle は光軸から外側コーンの縁までの半頂角の cos、立体角は 2π(1 - cos))
	double calculate_spot_light_solid_angle(double cos_half_angle);
	double convert_spot_light_lumen_to_candela(double lumen, double cos_half_angle);
	double convert_spot_light_candela_to_lumen(double candela, double cos_half_angle);

	//	分光視感度 = 1 の理想状態で測光量と放射量を変換する
	double convert_photometric_to_radiometric(double photometric_value);
	double convert_radiometric_to_photometric(double radiometric_value);

	//	黒体放射の色度(CIE 1931 xy)。Krystek (1985) のプランク軌跡の近似
	math::float2 get_temperature_chromaticity(int kelvin);

	//	色温度の色(Rec.2020 リニア、輝度 1 に正規化。色温度を変えても測光量が変わらない)
	math::vector3f get_temperature_color(int kelvin);

	//	Rec.2020 リニアの色を輝度 1 に正規化する
	math::vector3f normalize_luminance_rec2020(const math::vector3f& color);
}
