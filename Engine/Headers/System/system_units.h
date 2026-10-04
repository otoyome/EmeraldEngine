#pragma once

#include "Math/element.h"
#include <string>
#include <string_view>

class system_units
{
public:
	//	距離において、1.0f をどういう単位で取り扱うかを決める定数
	enum class length_units
	{
		micro_meter,
		milli_meter,
		centi_meter,
		meter,

		unit,	//	1.0unit = 10cm
	};

	//	質量について、1.0f とどのような単位で取り扱うかを決める定数
	enum class mass_units
	{
		micro_gram,
		milli_gram,
		gram,
		kilogram,

		unit,	//	1.0unit = 1kg
	};

	//	1秒間に何回更新するか
	static constexpr int frame_per_sec = 60;

	static constexpr std::string_view length_unit_text(length_units unit)
	{
		switch (unit)
		{
			case length_units::micro_meter:	return	"um";
			case length_units::milli_meter:	return	"mm";
			case length_units::centi_meter:	return	"cm";
			case length_units::meter:		return	"m";
			case length_units::unit:		return	"unit";
		}
		return	"unknwon";
	}

	static constexpr std::string_view mass_unit_text(mass_units unit)
	{
		switch (unit)
		{
			case mass_units::micro_gram:		return	"ug";
			case mass_units::milli_gram:		return	"mg";
			case mass_units::gram:			return	"g";
			case mass_units::kilogram:		return	"kg";
			case mass_units::unit:			return	"unit";
		}
		return	"unknwon";
	}

private:
	static constexpr float length_factor(length_units origin, length_units dest)
	{
		switch (origin)
		{
			case length_units::micro_meter:
			{
				switch (dest)
				{
				case length_units::milli_meter:	return 0.001f;
				case length_units::centi_meter:	return 1e-4f;
				case length_units::meter:		return 1e-6f;
				case length_units::unit:		return 1e-5f;

				}
				break;
			}
			case length_units::milli_meter:
			{
				switch (dest)
				{
					case length_units::micro_meter:	return 1000.0f;
					case length_units::centi_meter:	return 0.1f;
					case length_units::meter:		return 0.001f;
					case length_units::unit:		return 0.01f;
				}
				break;
			}
			case length_units::centi_meter:
			{
				switch (dest)
				{
					case length_units::micro_meter:	return 10000.0f;
					case length_units::milli_meter:	return 10.0f;
					case length_units::meter:		return 0.01f;
					case length_units::unit:		return 0.1f;
				}
				break;
			}
			case length_units::meter:
			{
				switch (dest)
				{
					case length_units::micro_meter:	return 1000000.0f;
					case length_units::milli_meter:	return 1000.0f;
					case length_units::centi_meter:	return 100.0f;
					case length_units::unit:		return 10.0f;
				}
				break;
			}
			case length_units::unit:
			{
				switch (dest)
				{
					case length_units::micro_meter:	return 100000.0f;
					case length_units::milli_meter:	return 100.0f;
					case length_units::centi_meter:	return 10.0f;
					case length_units::meter:		return 0.1f;
				}
				break;
			}
		}
		return 1.0f;
	}

	static constexpr float mass_factor(mass_units origin, mass_units dest)
	{
		switch (origin)
		{
			case mass_units::micro_gram:
			{
				switch (dest)
				{
					case mass_units::milli_gram:	return 0.001f;
					case mass_units::gram:			return 1e-6f;
					case mass_units::kilogram:		return 1e-9f;
					case mass_units::unit:			return 1e-9f;
				}
				break;
			}
			case mass_units::milli_gram:
			{
				switch (dest)
				{
					case mass_units::micro_gram:	return 1000.0f;
					case mass_units::gram:			return 0.001f;
					case mass_units::kilogram:		return 1e-6f;
					case mass_units::unit:			return 1e-6f;
				}
				break;
			}
			case mass_units::gram:
			{
				switch (dest)
				{
					case mass_units::micro_gram:	return 1000000.0f;
					case mass_units::milli_gram:	return 1000.0f;
					case mass_units::kilogram:		return 0.001f;
					case mass_units::unit:			return 0.001f;
				}
				break;
			}
			case mass_units::kilogram:
			{
				switch (dest)
				{
					case mass_units::micro_gram:	return 1000000000.0f;
					case mass_units::milli_gram:	return 1000000.0f;
					case mass_units::gram:			return 1000.0f;
					case mass_units::unit:			return 1.0f;
				}
				break;
			}
			case mass_units::unit:
			{
				switch (dest)
				{
					case mass_units::micro_gram:	return 1000000000.0f;
					case mass_units::milli_gram:	return 1000000.0f;
					case mass_units::gram:			return 1000.0f;
					case mass_units::kilogram:		return 1.0f;
				}
				break;
			}
		}
		return 1.0f;
	}
public:
	//	マイクロメートル <> ミリメートル変換
	static constexpr float convert_um_to_mm(float value) { return value * length_factor(length_units::micro_meter, length_units::milli_meter); }
	static constexpr float convert_mm_to_um(float value) { return value * length_factor(length_units::milli_meter, length_units::micro_meter); }

	//	ミリメートル <> センチメートル変換
	static constexpr float convert_mm_to_cm(float value) { return value * length_factor(length_units::milli_meter, length_units::centi_meter); }
	static constexpr float convert_cm_to_mm(float value) { return value * length_factor(length_units::centi_meter, length_units::milli_meter); }

	//	メートル <> センチメートル変換
	static constexpr float convert_cm_to_m(float value){ return value * length_factor(length_units::centi_meter, length_units::meter); }
	static constexpr float convert_m_to_cm(float value){ return value * length_factor(length_units::meter, length_units::centi_meter); }

	//	ミリメートル <> メートル変換
	static constexpr float convert_mm_to_m(float value) { return value * length_factor(length_units::milli_meter, length_units::meter); }
	static constexpr float convert_m_to_mm(float value) { return value * length_factor(length_units::meter, length_units::milli_meter); }

	//	各単位系での距離をゲーム単位へ変換
	static constexpr float convert_um_to_unit(float value) { return value * length_factor(length_units::micro_meter, length_units::unit); }
	static constexpr float convert_mm_to_unit(float value) { return value * length_factor(length_units::milli_meter, length_units::unit); }
	static constexpr float convert_cm_to_unit(float value) { return value * length_factor(length_units::centi_meter, length_units::unit); }
	static constexpr float convert_m_to_unit(float value) { return value * length_factor(length_units::meter, length_units::unit); }
	static constexpr float convert_unit_to_um(float value) { return value * length_factor(length_units::unit, length_units::micro_meter); }
	static constexpr float convert_unit_to_mm(float value) { return value * length_factor(length_units::unit, length_units::milli_meter); }
	static constexpr float convert_unit_to_cm(float value) { return value * length_factor(length_units::unit, length_units::centi_meter); }
	static constexpr float convert_unit_to_m(float value) { return value * length_factor(length_units::unit, length_units::meter); }

	static constexpr math::float2 convert_m_to_unit(math::float2 value)
	{
		return {convert_m_to_unit(value.x), convert_m_to_unit(value.y)};
	}
	static constexpr math::float3 convert_m_to_unit(math::float3 value)
	{
		return {convert_m_to_unit(value.x), convert_m_to_unit(value.y), convert_m_to_unit(value.z)};
	}
	static constexpr math::float4 convert_m_to_unit(math::float4 value)
	{
		return {convert_m_to_unit(value.x), convert_m_to_unit(value.y), convert_m_to_unit(value.z), convert_m_to_unit(value.w)};
	}

public:
	//	マイクログラムの<>ミリグラム変換
	static constexpr float convert_ug_to_mg(float value) { return value * mass_factor(mass_units::micro_gram, mass_units::milli_gram); }
	static constexpr float convert_mg_to_ug(float value) { return value * mass_factor(mass_units::milli_gram, mass_units::micro_gram); }

	//	ミリグラムの<>グラム変換
	static constexpr float convert_mg_to_g(float value) { return value * mass_factor(mass_units::milli_gram, mass_units::gram); }
	static constexpr float convert_g_to_mg(float value) { return value * mass_factor(mass_units::gram, mass_units::milli_gram); }

	//	グラムの<>キログラム変換
	static constexpr float convert_g_to_kg(float value) { return value * mass_factor(mass_units::gram, mass_units::kilogram); }
	static constexpr float convert_kg_to_g(float value) { return value * mass_factor(mass_units::kilogram, mass_units::gram); }

	//	各単位系での質量をゲーム単位へ変換
	static constexpr float convert_ug_to_unit(float value) { return value * mass_factor(mass_units::micro_gram, mass_units::unit); }
	static constexpr float convert_mg_to_unit(float value) { return value * mass_factor(mass_units::milli_gram, mass_units::unit); }
	static constexpr float convert_g_to_unit(float value) { return value * mass_factor(mass_units::gram, mass_units::unit); }
	static constexpr float convert_kg_to_unit(float value) { return value * mass_factor(mass_units::kilogram, mass_units::unit); }
	static constexpr float convert_unit_to_ug(float value) { return value * mass_factor(mass_units::unit, mass_units::micro_gram); }
	static constexpr float convert_unit_to_mg(float value) { return value * mass_factor(mass_units::unit, mass_units::milli_gram); }
	static constexpr float convert_unit_to_g(float value) { return value * mass_factor(mass_units::unit, mass_units::gram); }
	static constexpr float convert_unit_to_kg(float value) { return value * mass_factor(mass_units::unit, mass_units::kilogram); }

	//	秒 <> ミリ秒変換
	static constexpr float convert_sec_to_millisec(float value) { return value * 1000.0f; }
	static constexpr float convert_millisec_to_sec(float value) { return value / 1000.0f; }
	//	秒 <> 分変換
	static constexpr float convert_sec_to_minute(float value) { return value * 60.0f; }
	static constexpr float convert_minute_to_sec(float value) { return value / 60.0f; }
	//	秒 <> 時変換
	static constexpr float convert_sec_to_hour(float value) { return value * 3600.0f; }
	static constexpr float convert_hour_to_sec(float value) { return value / 3600.0f; }

	//	フレーム <> 秒
	static constexpr float convert_sec_to_fps(float value) { return value * frame_per_sec; }
	static constexpr float convert_fps_to_sec(float value) { return value / frame_per_sec; }
};
