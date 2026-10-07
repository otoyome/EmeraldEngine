#pragma once

#include "Math/element.h"
#include <concepts>
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
	//	係数は double リテラルで定義し、T へキャストして返す
	//	(float の場合は 1e-4f と同じ値になる)
	template<std::floating_point T>
	static constexpr T length_factor(length_units origin, length_units dest)
	{
		switch (origin)
		{
			case length_units::micro_meter:
			{
				switch (dest)
				{
					case length_units::milli_meter:	return static_cast<T>(0.001);
					case length_units::centi_meter:	return static_cast<T>(1e-4);
					case length_units::meter:		return static_cast<T>(1e-6);
					case length_units::unit:		return static_cast<T>(1e-5);
					default: break;
				}
				break;
			}
			case length_units::milli_meter:
			{
				switch (dest)
				{
					case length_units::micro_meter:	return static_cast<T>(1000.0);
					case length_units::centi_meter:	return static_cast<T>(0.1);
					case length_units::meter:		return static_cast<T>(0.001);
					case length_units::unit:		return static_cast<T>(0.01);
					default: break;
				}
				break;
			}
			case length_units::centi_meter:
			{
				switch (dest)
				{
					case length_units::micro_meter:	return static_cast<T>(10000.0);
					case length_units::milli_meter:	return static_cast<T>(10.0);
					case length_units::meter:		return static_cast<T>(0.01);
					case length_units::unit:		return static_cast<T>(0.1);
					default: break;
				}
				break;
			}
			case length_units::meter:
			{
				switch (dest)
				{
					case length_units::micro_meter:	return static_cast<T>(1000000.0);
					case length_units::milli_meter:	return static_cast<T>(1000.0);
					case length_units::centi_meter:	return static_cast<T>(100.0);
					case length_units::unit:		return static_cast<T>(10.0);
					default: break;
				}
				break;
			}
			case length_units::unit:
			{
				switch (dest)
				{
					case length_units::micro_meter:	return static_cast<T>(100000.0);
					case length_units::milli_meter:	return static_cast<T>(100.0);
					case length_units::centi_meter:	return static_cast<T>(10.0);
					case length_units::meter:		return static_cast<T>(0.1);
					default: break;
				}
				break;
			}
		}
		return static_cast<T>(1.0);
	}

	template<std::floating_point T>
	static constexpr T mass_factor(mass_units origin, mass_units dest)
	{
		switch (origin)
		{
			case mass_units::micro_gram:
			{
				switch (dest)
				{
					case mass_units::milli_gram:	return static_cast<T>(0.001);
					case mass_units::gram:			return static_cast<T>(1e-6);
					case mass_units::kilogram:		return static_cast<T>(1e-9);
					case mass_units::unit:			return static_cast<T>(1e-9);
					default: break;
				}
				break;
			}
			case mass_units::milli_gram:
			{
				switch (dest)
				{
					case mass_units::micro_gram:	return static_cast<T>(1000.0);
					case mass_units::gram:			return static_cast<T>(0.001);
					case mass_units::kilogram:		return static_cast<T>(1e-6);
					case mass_units::unit:			return static_cast<T>(1e-6);
					default: break;
				}
				break;
			}
			case mass_units::gram:
			{
				switch (dest)
				{
					case mass_units::micro_gram:	return static_cast<T>(1000000.0);
					case mass_units::milli_gram:	return static_cast<T>(1000.0);
					case mass_units::kilogram:		return static_cast<T>(0.001);
					case mass_units::unit:			return static_cast<T>(0.001);
					default: break;
				}
				break;
			}
			case mass_units::kilogram:
			{
				switch (dest)
				{
					case mass_units::micro_gram:	return static_cast<T>(1000000000.0);
					case mass_units::milli_gram:	return static_cast<T>(1000000.0);
					case mass_units::gram:			return static_cast<T>(1000.0);
					case mass_units::unit:			return static_cast<T>(1.0);
					default: break;
				}
				break;
			}
			case mass_units::unit:
			{
				switch (dest)
				{
					case mass_units::micro_gram:	return static_cast<T>(1000000000.0);
					case mass_units::milli_gram:	return static_cast<T>(1000000.0);
					case mass_units::gram:			return static_cast<T>(1000.0);
					case mass_units::kilogram:		return static_cast<T>(1.0);
					default: break;
				}
				break;
			}
		}
		return static_cast<T>(1.0);
	}

public:
	//	マイクロメートル <> ミリメートル変換
	template<std::floating_point T> static constexpr T convert_um_to_mm(T value) { return value * length_factor<T>(length_units::micro_meter, length_units::milli_meter); }
	template<std::floating_point T> static constexpr T convert_mm_to_um(T value) { return value * length_factor<T>(length_units::milli_meter, length_units::micro_meter); }

	//	ミリメートル <> センチメートル変換
	template<std::floating_point T> static constexpr T convert_mm_to_cm(T value) { return value * length_factor<T>(length_units::milli_meter, length_units::centi_meter); }
	template<std::floating_point T> static constexpr T convert_cm_to_mm(T value) { return value * length_factor<T>(length_units::centi_meter, length_units::milli_meter); }

	//	メートル <> センチメートル変換
	template<std::floating_point T> static constexpr T convert_cm_to_m(T value) { return value * length_factor<T>(length_units::centi_meter, length_units::meter); }
	template<std::floating_point T> static constexpr T convert_m_to_cm(T value) { return value * length_factor<T>(length_units::meter, length_units::centi_meter); }

	//	ミリメートル <> メートル変換
	template<std::floating_point T> static constexpr T convert_mm_to_m(T value) { return value * length_factor<T>(length_units::milli_meter, length_units::meter); }
	template<std::floating_point T> static constexpr T convert_m_to_mm(T value) { return value * length_factor<T>(length_units::meter, length_units::milli_meter); }

	//	各単位系での距離をゲーム単位へ変換
	template<std::floating_point T> static constexpr T convert_um_to_unit(T value) { return value * length_factor<T>(length_units::micro_meter, length_units::unit); }
	template<std::floating_point T> static constexpr T convert_mm_to_unit(T value) { return value * length_factor<T>(length_units::milli_meter, length_units::unit); }
	template<std::floating_point T> static constexpr T convert_cm_to_unit(T value) { return value * length_factor<T>(length_units::centi_meter, length_units::unit); }
	template<std::floating_point T> static constexpr T convert_m_to_unit(T value) { return value * length_factor<T>(length_units::meter, length_units::unit); }
	template<std::floating_point T> static constexpr T convert_unit_to_um(T value) { return value * length_factor<T>(length_units::unit, length_units::micro_meter); }
	template<std::floating_point T> static constexpr T convert_unit_to_mm(T value) { return value * length_factor<T>(length_units::unit, length_units::milli_meter); }
	template<std::floating_point T> static constexpr T convert_unit_to_cm(T value) { return value * length_factor<T>(length_units::unit, length_units::centi_meter); }
	template<std::floating_point T> static constexpr T convert_unit_to_m(T value) { return value * length_factor<T>(length_units::unit, length_units::meter); }

	//	ベクトル版
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
	static constexpr math::double2 convert_m_to_unit(math::double2 value)
	{
		return {convert_m_to_unit(value.x), convert_m_to_unit(value.y)};
	}
	static constexpr math::double3 convert_m_to_unit(math::double3 value)
	{
		return {convert_m_to_unit(value.x), convert_m_to_unit(value.y), convert_m_to_unit(value.z)};
	}
	static constexpr math::double4 convert_m_to_unit(math::double4 value)
	{
		return {convert_m_to_unit(value.x), convert_m_to_unit(value.y), convert_m_to_unit(value.z), convert_m_to_unit(value.w)};
	}

public:
	//	マイクログラムの<>ミリグラム変換
	template<std::floating_point T> static constexpr T convert_ug_to_mg(T value) { return value * mass_factor<T>(mass_units::micro_gram, mass_units::milli_gram); }
	template<std::floating_point T> static constexpr T convert_mg_to_ug(T value) { return value * mass_factor<T>(mass_units::milli_gram, mass_units::micro_gram); }

	//	ミリグラムの<>グラム変換
	template<std::floating_point T> static constexpr T convert_mg_to_g(T value) { return value * mass_factor<T>(mass_units::milli_gram, mass_units::gram); }
	template<std::floating_point T> static constexpr T convert_g_to_mg(T value) { return value * mass_factor<T>(mass_units::gram, mass_units::milli_gram); }

	//	グラムの<>キログラム変換
	template<std::floating_point T> static constexpr T convert_g_to_kg(T value) { return value * mass_factor<T>(mass_units::gram, mass_units::kilogram); }
	template<std::floating_point T> static constexpr T convert_kg_to_g(T value) { return value * mass_factor<T>(mass_units::kilogram, mass_units::gram); }

	//	各単位系での質量をゲーム単位へ変換
	template<std::floating_point T> static constexpr T convert_ug_to_unit(T value) { return value * mass_factor<T>(mass_units::micro_gram, mass_units::unit); }
	template<std::floating_point T> static constexpr T convert_mg_to_unit(T value) { return value * mass_factor<T>(mass_units::milli_gram, mass_units::unit); }
	template<std::floating_point T> static constexpr T convert_g_to_unit(T value) { return value * mass_factor<T>(mass_units::gram, mass_units::unit); }
	template<std::floating_point T> static constexpr T convert_kg_to_unit(T value) { return value * mass_factor<T>(mass_units::kilogram, mass_units::unit); }
	template<std::floating_point T> static constexpr T convert_unit_to_ug(T value) { return value * mass_factor<T>(mass_units::unit, mass_units::micro_gram); }
	template<std::floating_point T> static constexpr T convert_unit_to_mg(T value) { return value * mass_factor<T>(mass_units::unit, mass_units::milli_gram); }
	template<std::floating_point T> static constexpr T convert_unit_to_g(T value) { return value * mass_factor<T>(mass_units::unit, mass_units::gram); }
	template<std::floating_point T> static constexpr T convert_unit_to_kg(T value) { return value * mass_factor<T>(mass_units::unit, mass_units::kilogram); }

	//	秒 <> ミリ秒変換
	template<std::floating_point T> static constexpr T convert_sec_to_millisec(T value) { return value * static_cast<T>(1000); }
	template<std::floating_point T> static constexpr T convert_millisec_to_sec(T value) { return value / static_cast<T>(1000); }
	//	秒 <> 分変換
	template<std::floating_point T> static constexpr T convert_sec_to_minute(T value) { return value * static_cast<T>(60); }
	template<std::floating_point T> static constexpr T convert_minute_to_sec(T value) { return value / static_cast<T>(60); }
	//	秒 <> 時変換
	template<std::floating_point T> static constexpr T convert_sec_to_hour(T value) { return value * static_cast<T>(3600); }
	template<std::floating_point T> static constexpr T convert_hour_to_sec(T value) { return value / static_cast<T>(3600); }

	//	フレーム <> 秒
	template<std::floating_point T> static constexpr T convert_sec_to_fps(T value) { return value * static_cast<T>(frame_per_sec); }
	template<std::floating_point T> static constexpr T convert_fps_to_sec(T value) { return value / static_cast<T>(frame_per_sec); }
};
