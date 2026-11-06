#include "Graphics/exposure_controller.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{
	//	APEX 値とその範囲
	struct apex_values
	{
		float Av;
		float Tv;
		float Sv;
	};
	struct apex_range
	{
		float minimum;
		float maximum;
	};

	enum class exposure_parameters
	{
		f_number,
		shutter_speed,
		iso_speed,
	};

	//	Av + Tv - Sv が target になるように order の順に値を動かす
	void distribute_exposure(apex_values& apex, float target, const apex_range (&ranges)[3], std::initializer_list<exposure_parameters> order)
	{
		for (auto parameter : order)
		{
			const float remain = target - (apex.Av + apex.Tv - apex.Sv);
			const apex_range& range = ranges[static_cast<int>(parameter)];
			switch (parameter)
			{
				case exposure_parameters::f_number:			apex.Av = std::clamp(apex.Av + remain, range.minimum, range.maximum); break;
				case exposure_parameters::shutter_speed:	apex.Tv = std::clamp(apex.Tv + remain, range.minimum, range.maximum); break;
				case exposure_parameters::iso_speed:		apex.Sv = std::clamp(apex.Sv - remain, range.minimum, range.maximum); break;
			}
		}
	}
}

void exposure_controller::reset(const exposure_settings& settings)
{
	f_number = std::clamp(settings.f_number, f_number_minimum, f_number_maximum);
	shutter_speed = std::clamp(settings.shutter_speed, shutter_speed_minimum, shutter_speed_maximum);
	iso_speed = std::clamp(settings.iso_speed, iso_speed_minimum, iso_speed_maximum);
}

void exposure_controller::update(const exposure_settings& settings, float average_luminance, float elapsed_time)
{
	using modes = exposure_settings::auto_exposure_modes;
	if (settings.auto_exposure_mode == modes::none)
	{
		reset(settings);
		return;
	}
	if (!(average_luminance > 0.0f))
		return;

	const apex_values current = { calculate_Av(f_number), calculate_Tv(shutter_speed), calculate_Sv(iso_speed) };
	const apex_range ranges[3] =
	{
		{ calculate_Av(f_number_minimum), calculate_Av(f_number_maximum) },
		{ calculate_Tv(shutter_speed_maximum), calculate_Tv(shutter_speed_minimum) },
		{ calculate_Sv(iso_speed_minimum), calculate_Sv(iso_speed_maximum) },
	};

	//	測光した輝度に合う露出(露出補正の分だけ明るくする)
	const float target_exposure = calculate_Bv(average_luminance) - settings.exposure_compensation;

	apex_values target = current;
	switch (settings.auto_exposure_mode)
	{
		case modes::f_number:
			//	ISO感度はベース感度に戻し、F値で吸収しきれない分だけ動かす
			target.Sv = std::clamp(calculate_Sv(base_iso_speed), ranges[2].minimum, ranges[2].maximum);
			distribute_exposure(target, target_exposure, ranges, { exposure_parameters::f_number, exposure_parameters::iso_speed });
			break;
		case modes::shutter_speed:
			target.Sv = std::clamp(calculate_Sv(base_iso_speed), ranges[2].minimum, ranges[2].maximum);
			distribute_exposure(target, target_exposure, ranges, { exposure_parameters::shutter_speed, exposure_parameters::iso_speed });
			break;
		case modes::iso_speed:
			distribute_exposure(target, target_exposure, ranges, { exposure_parameters::iso_speed });
			break;
		default:
			break;
	}

	//	時間をかけて適応する
	const float rate = (settings.adaptation_time > 0.0f) ? 1.0f - std::exp(-elapsed_time / settings.adaptation_time) : 1.0f;
	f_number = calculate_f_number(current.Av + (target.Av - current.Av) * rate);
	shutter_speed = calculate_shutter_speed(current.Tv + (target.Tv - current.Tv) * rate);
	iso_speed = calculate_iso_speed(current.Sv + (target.Sv - current.Sv) * rate);
}

float exposure_controller::calculate_exposure_scale() const
{
	//	センサーが飽和する輝度 L_sat = 78 / q × 2^(Av + Tv) / ISO感度
	const float brightness = (saturation_value / lens_imaging_factor) * std::exp2(calculate_Av(f_number) + calculate_Tv(shutter_speed)) / iso_speed;
	return 1.0f / brightness;
}

float exposure_controller::calculate_Av(float f_number)
{
	return 2.0f * std::log2(f_number);
}

float exposure_controller::calculate_f_number(float Av)
{
	return std::exp2(Av * 0.5f);
}

float exposure_controller::calculate_Tv(float shutter_speed)
{
	return -std::log2(shutter_speed);
}

float exposure_controller::calculate_shutter_speed(float Tv)
{
	return std::exp2(-Tv);
}

float exposure_controller::calculate_Sv(float iso_speed)
{
	return std::log2(iso_speed * sensitivity_value_n);
}

float exposure_controller::calculate_iso_speed(float Sv)
{
	return std::exp2(Sv) / sensitivity_value_n;
}

float exposure_controller::calculate_Bv(float luminance)
{
	return std::log2((std::max)(luminance, 1e-6f) / (sensitivity_value_n * sensitivity_value_k));
}
