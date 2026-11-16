#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

#include "color_space.hlsli"

#define	alignment( s, a )		( ( s + ( a - 1 ) ) & ~( a - 1 ) )

#define MEDIUMP_FLT_MAX    65504.0
#define saturate_mediump(x) min(x, MEDIUMP_FLT_MAX)

float map_range(float value, float from_min = 0.5f, float from_max = 1.0f, float to_min = 0.0f, float to_max = 1.0f)
{
    return  to_min + (value - from_min) * ((to_max - to_min) / max(from_max - from_min, 0.0001f));
}

float smooth_greator_or_equal(float value, float push, float smoothness)
{
    // T == 1 → カクッとした変化を返す
    // T == 0 → スムーズな変化を返す
    float   A   = map_range(value, push, push + smoothness);
    float   B   = value >= push;
    float   T   = abs(0.0f - smoothness) <= 0.0f;
    return  lerp(A, B, T);
}

static const uint dither_pattern_16 = 0;
static const uint dither_pattern_64 = 1;
float calculate_alpha_dithering(float2 screen_position, uint dither_pattern_index = dither_pattern_64)
{
#if 01
	//	Interleaved Gradient Noise（Jimenez 2014）
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(screen_position, magic.xy)));
#else
    float dither = 0.0f;
    uint sx = (int) screen_position.x;
    uint sy = (int) screen_position.y;
    uint x = 0, y = 0;
    switch(dither_pattern_index)
    {
        case dither_pattern_16:
        {
                static const float dither_pattern[16] =
            {
                 0.0f / 16.0f,  8.0f / 16.0f,  2.0f / 16.0f, 10.0f / 16.0f,
                12.0f / 16.0f,  4.0f / 16.0f, 14.0f / 16.0f,  6.0f / 16.0f,
                 3.0f / 16.0f, 11.0f / 16.0f,  1.0f / 16.0f,  9.0f / 16.0f,
                15.0f / 16.0f,  7.0f / 16.0f, 13.0f / 16.0f,  5.0f / 16.0f
            };
            x = sx % 4;
            y = sy % 4;
            dither = (float) dither_pattern[x + y * 4];
            break;
        }
        default:
        case dither_pattern_64:
        {
            static const float dither_pattern[64] =
            {
                 0.0f / 64.0f, 48.0f / 64.0f, 12.0f / 64.0f, 60.0f / 64.0f,  3.0f / 64.0f, 51.0f / 64.0f, 15.0f / 64.0f, 63.0f / 64.0f,
                32.0f / 64.0f, 16.0f / 64.0f, 44.0f / 64.0f, 28.0f / 64.0f, 35.0f / 64.0f, 19.0f / 64.0f, 47.0f / 64.0f, 31.0f / 64.0f,
                 8.0f / 64.0f, 56.0f / 64.0f,  4.0f / 64.0f, 52.0f / 64.0f, 11.0f / 64.0f, 59.0f / 64.0f,  7.0f / 64.0f, 55.0f / 64.0f,
                40.0f / 64.0f, 24.0f / 64.0f, 36.0f / 64.0f, 20.0f / 64.0f, 43.0f / 64.0f, 27.0f / 64.0f, 39.0f / 64.0f, 23.0f / 64.0f,
                 2.0f / 64.0f, 50.0f / 64.0f, 14.0f / 64.0f, 62.0f / 64.0f,  1.0f / 64.0f, 49.0f / 64.0f, 13.0f / 64.0f, 61.0f / 64.0f,
                34.0f / 64.0f, 18.0f / 64.0f, 46.0f / 64.0f, 30.0f / 64.0f, 33.0f / 64.0f, 17.0f / 64.0f, 45.0f / 64.0f, 29.0f / 64.0f,
                10.0f / 64.0f, 58.0f / 64.0f,  6.0f / 64.0f, 54.0f / 64.0f,  9.0f / 64.0f, 57.0f / 64.0f,  5.0f / 64.0f, 53.0f / 64.0f,
                42.0f / 64.0f, 26.0f / 64.0f, 38.0f / 64.0f, 22.0f / 64.0f, 41.0f / 64.0f, 25.0f / 64.0f, 37.0f / 64.0f, 21.0f / 64.0f
                };
            x = sx % 8;
            y = sy % 8;
            dither = (float) dither_pattern[x + y * 8];
            break;
        }
    }
    return dither;
#endif
}

float random(float2 seeds)
{
    return frac(sin(dot(seeds, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float block_noise(float2 seeds)
{
    return random(floor(seeds));
}

float repeat(float _max, float _value)
{
    float t = fmod(_value, _max);
    return t < 0.0f ? _max + t : t;
}
 
float2 repeat(float2 _max, float2 _value)
{
    float2 t = fmod(_value, _max);
    return t < 0.0f ? _max + t : t; // componentwise ternary (SM4+)
}
 
float3 repeat(float3 _max, float3 _value)
{
    float3 t = fmod(_value, _max);
    return t < 0.0f ? _max + t : t;
}
 
float4 repeat(float4 _max, float4 _value)
{
    float4 t = fmod(_value, _max);
    return t < 0.0f ? _max + t : t;
}

float wrap(float _min, float _max, float _value)
{
    return repeat(_max - _min, _value - _min) + _min;
}
 
float2 wrap(float2 _min, float2 _max, float2 _value)
{
    return repeat(_max - _min, _value - _min) + _min;
}
 
float3 wrap(float3 _min, float3 _max, float3 _value)
{
    return repeat(_max - _min, _value - _min) + _min;
}
 
float4 wrap(float4 _min, float4 _max, float4 _value)
{
    return repeat(_max - _min, _value - _min) + _min;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  深度0(遠クリップ/無限遠)はw=0になり割れないので、奥の深度で逆射影する
static const float unproject_depth = 1.0e-7f;

//	ビュー空間の深度の上限。無限遠の空はこの値で表す
static const float gbuffer_max_linear_depth = 1.0e10f;

//	射影深度のnear面/far面(無限遠)の値。
//	@remainder	CPU側のcommon_pipeline_statesと値を合わせること
static const float depth_near_value = 1.0f;
static const float depth_far_value = 0.0f;

//	線形深度から射影深度へ戻す際の下限
static const float min_projection_depth = 1.0e-20f;

//	線形深度の相対差を求める際の0除算防止
static const float relative_linear_depth_epsilon = 1.0e-6f;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	距離単位
static const int length_units_micro_meter = 0;
static const int length_units_milli_meter = 1;
static const int length_units_centi_meter = 2;
static const int length_units_meter = 3;
static const int length_units_unit = 4;	//	基準単位 1.0unit = 10cm

//	指定の単位に変換
float convert_length_factor(int origin, int dest)
{
	switch (origin)
	{
		case length_units_micro_meter:
		{
			switch (dest)
			{
				case length_units_milli_meter: return 0.001f;
				case length_units_centi_meter: return 1e-4f;
				case length_units_meter: return 1e-6f;
				case length_units_unit: return 1e-5f;
			}
			break;
		}
		case length_units_milli_meter:
		{
			switch (dest)
			{
				case length_units_micro_meter: return 1000.0f;
				case length_units_centi_meter: return 0.1f;
				case length_units_meter: return 0.001f;
				case length_units_unit: return 0.01f;
			}
			break;
		}
		case length_units_centi_meter:
		{
			switch (dest)
			{
				case length_units_micro_meter: return 10000.0f;
				case length_units_milli_meter: return 10.0f;
				case length_units_meter: return 0.01f;
				case length_units_unit: return 0.1f;
			}
			break;
		}
		case length_units_meter:
		{
			switch (dest)
			{
				case length_units_micro_meter: return 1000000.0f;
				case length_units_milli_meter: return 1000.0f;
				case length_units_centi_meter: return 100.0f;
				case length_units_unit: return 10.0f;
			}
			break;
		}
		case length_units_unit:
		{
			switch (dest)
			{
				case length_units_micro_meter: return 100000.0f;
				case length_units_milli_meter: return 100.0f;
				case length_units_centi_meter: return 10.0f;
				case length_units_meter: return 0.1f;
			}
			break;
		}
	}
	return 1.0f;
}

//	マイクロメートル <> ミリメートル変換
float convert_um_to_mm(float value)
{
	return value * convert_length_factor(length_units_micro_meter, length_units_milli_meter);
}
float convert_mm_to_um(float value)
{
	return value * convert_length_factor(length_units_milli_meter, length_units_micro_meter);
}

//	ミリメートル <> センチメートル変換
float convert_mm_to_cm(float value)
{
	return value * convert_length_factor(length_units_milli_meter, length_units_centi_meter);
}
float convert_cm_to_mm(float value)
{
	return value * convert_length_factor(length_units_centi_meter, length_units_milli_meter);
}

//	メートル <> センチメートル変換
float convert_cm_to_m(float value)
{
	return value * convert_length_factor(length_units_centi_meter, length_units_meter);
}
float convert_m_to_cm(float value)
{
	return value * convert_length_factor(length_units_meter, length_units_centi_meter);
}

//	ミリメートル <> メートル変換
float convert_mm_to_m(float value)
{
	return value * convert_length_factor(length_units_milli_meter, length_units_meter);
}
float convert_m_to_mm(float value)
{
	return value * convert_length_factor(length_units_meter, length_units_milli_meter);
}

//	各単位系での距離をゲーム単位へ変換
float convert_um_to_unit(float value)
{
	return value * convert_length_factor(length_units_micro_meter, length_units_unit);
}
float convert_mm_to_unit(float value)
{
	return value * convert_length_factor(length_units_milli_meter, length_units_unit);
}
float convert_cm_to_unit(float value)
{
	return value * convert_length_factor(length_units_centi_meter, length_units_unit);
}
float convert_m_to_unit(float value)
{
	return value * convert_length_factor(length_units_meter, length_units_unit);
}
float convert_unit_to_um(float value)
{
	return value * convert_length_factor(length_units_unit, length_units_micro_meter);
}
float convert_unit_to_mm(float value)
{
	return value * convert_length_factor(length_units_unit, length_units_milli_meter);
}
float convert_unit_to_cm(float value)
{
	return value * convert_length_factor(length_units_unit, length_units_centi_meter);
}
float convert_unit_to_m(float value)
{
	return value * convert_length_factor(length_units_unit, length_units_meter);
}

//	ベクトル版
float2 convert_um_to_unit(float2 value) { return value * convert_length_factor(length_units_micro_meter, length_units_unit); }
float3 convert_um_to_unit(float3 value) { return value * convert_length_factor(length_units_micro_meter, length_units_unit); }
float4 convert_um_to_unit(float4 value) { return value * convert_length_factor(length_units_micro_meter, length_units_unit); }
float2 convert_mm_to_unit(float2 value) { return value * convert_length_factor(length_units_milli_meter, length_units_unit); }
float3 convert_mm_to_unit(float3 value) { return value * convert_length_factor(length_units_milli_meter, length_units_unit); }
float4 convert_mm_to_unit(float4 value) { return value * convert_length_factor(length_units_milli_meter, length_units_unit); }
float2 convert_cm_to_unit(float2 value) { return value * convert_length_factor(length_units_centi_meter, length_units_unit); }
float3 convert_cm_to_unit(float3 value) { return value * convert_length_factor(length_units_centi_meter, length_units_unit); }
float4 convert_cm_to_unit(float4 value) { return value * convert_length_factor(length_units_centi_meter, length_units_unit); }
float2 convert_m_to_unit(float2 value) { return value * convert_length_factor(length_units_meter, length_units_unit); }
float3 convert_m_to_unit(float3 value) { return value * convert_length_factor(length_units_meter, length_units_unit); }
float4 convert_m_to_unit(float4 value) { return value * convert_length_factor(length_units_meter, length_units_unit); }
float2 convert_unit_to_um(float2 value) { return value * convert_length_factor(length_units_unit, length_units_micro_meter); }
float3 convert_unit_to_um(float3 value) { return value * convert_length_factor(length_units_unit, length_units_micro_meter); }
float4 convert_unit_to_um(float4 value) { return value * convert_length_factor(length_units_unit, length_units_micro_meter); }
float2 convert_unit_to_mm(float2 value) { return value * convert_length_factor(length_units_unit, length_units_milli_meter); }
float3 convert_unit_to_mm(float3 value) { return value * convert_length_factor(length_units_unit, length_units_milli_meter); }
float4 convert_unit_to_mm(float4 value) { return value * convert_length_factor(length_units_unit, length_units_milli_meter); }
float2 convert_unit_to_cm(float2 value) { return value * convert_length_factor(length_units_unit, length_units_centi_meter); }
float3 convert_unit_to_cm(float3 value) { return value * convert_length_factor(length_units_unit, length_units_centi_meter); }
float4 convert_unit_to_cm(float4 value) { return value * convert_length_factor(length_units_unit, length_units_centi_meter); }
float2 convert_unit_to_m(float2 value) { return value * convert_length_factor(length_units_unit, length_units_meter); }
float3 convert_unit_to_m(float3 value) { return value * convert_length_factor(length_units_unit, length_units_meter); }
float4 convert_unit_to_m(float4 value) { return value * convert_length_factor(length_units_unit, length_units_meter); }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	質量単位
static const int mass_units_micro_gram = 0;
static const int mass_units_milli_gram = 1;
static const int mass_units_gram = 2;
static const int mass_units_kilogram = 3;
static const int mass_units_unit = 4;	//	基準単位 1.0unit = 1kg

//	指定の単位に変換
float convert_mass_factor(int origin, int dest)
{
	switch (origin)
	{
		case mass_units_micro_gram:
		{
			switch (dest)
			{
				case mass_units_milli_gram: return 0.001f;
				case mass_units_gram: return 1e-6f;
				case mass_units_kilogram: return 1e-9f;
				case mass_units_unit: return 1e-9f;
			}
			break;
		}
		case mass_units_milli_gram:
		{
			switch (dest)
			{
				case mass_units_micro_gram: return 1000.0f;
				case mass_units_gram: return 0.001f;
				case mass_units_kilogram: return 1e-6f;
				case mass_units_unit: return 1e-6f;
			}
			break;
		}
		case mass_units_gram:
		{
			switch (dest)
			{
				case mass_units_micro_gram: return 1000000.0f;
				case mass_units_milli_gram: return 1000.0f;
				case mass_units_kilogram: return 0.001f;
				case mass_units_unit: return 0.001f;
			}
			break;
		}
		case mass_units_kilogram:
		{
			switch (dest)
			{
				case mass_units_micro_gram: return 1000000000.0f;
				case mass_units_milli_gram: return 1000000.0f;
				case mass_units_gram: return 1000.0f;
				case mass_units_unit: return 1.0f;
			}
			break;
		}
		case mass_units_unit:
		{
			switch (dest)
			{
				case mass_units_micro_gram: return 1000000000.0f;
				case mass_units_milli_gram: return 1000000.0f;
				case mass_units_gram: return 1000.0f;
				case mass_units_kilogram: return 1.0f;
			}
			break;
		}
	}
	return 1.0f;
}

//	マイクログラム <> ミリグラム変換
float convert_ug_to_mg(float value)
{
	return value * convert_mass_factor(mass_units_micro_gram, mass_units_milli_gram);
}
float convert_mg_to_ug(float value)
{
	return value * convert_mass_factor(mass_units_milli_gram, mass_units_micro_gram);
}

//	ミリグラム <> グラム変換
float convert_mg_to_g(float value)
{
	return value * convert_mass_factor(mass_units_milli_gram, mass_units_gram);
}
float convert_g_to_mg(float value)
{
	return value * convert_mass_factor(mass_units_gram, mass_units_milli_gram);
}

//	グラム <> キログラム変換
float convert_g_to_kg(float value)
{
	return value * convert_mass_factor(mass_units_gram, mass_units_kilogram);
}
float convert_kg_to_g(float value)
{
	return value * convert_mass_factor(mass_units_kilogram, mass_units_gram);
}

//	各単位系での質量をゲーム単位へ変換
float convert_ug_to_unit(float value)
{
	return value * convert_mass_factor(mass_units_micro_gram, mass_units_unit);
}
float convert_mg_to_unit(float value)
{
	return value * convert_mass_factor(mass_units_milli_gram, mass_units_unit);
}
float convert_g_to_unit(float value)
{
	return value * convert_mass_factor(mass_units_gram, mass_units_unit);
}
float convert_kg_to_unit(float value)
{
	return value * convert_mass_factor(mass_units_kilogram, mass_units_unit);
}
float convert_unit_to_ug(float value)
{
	return value * convert_mass_factor(mass_units_unit, mass_units_micro_gram);
}
float convert_unit_to_mg(float value)
{
	return value * convert_mass_factor(mass_units_unit, mass_units_milli_gram);
}
float convert_unit_to_g(float value)
{
	return value * convert_mass_factor(mass_units_unit, mass_units_gram);
}
float convert_unit_to_kg(float value)
{
	return value * convert_mass_factor(mass_units_unit, mass_units_kilogram);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	距離単位
static const int frame_per_sec = 60;
//	秒 <> ミリ秒変換
float convert_sec_to_millisec(float value)
{
	return value * 1000.0f;
}
float convert_millisec_to_sec(float value)
{
	return value / 1000.0f;
}

//	秒 <> 分変換
float convert_sec_to_minute(float value)
{
	return value / 60.0f;
}
float convert_minute_to_sec(float value)
{
	return value * 60.0f;
}

//	秒 <> 時変換
float convert_sec_to_hour(float value)
{
	return value / 3600.0f;
}
float convert_hour_to_sec(float value)
{
	return value * 3600.0f;
}

//	フレーム <> 秒
float convert_sec_to_fps(float value)
{
	return value * frame_per_sec;
}
float convert_fps_to_sec(float value)
{
	return value / frame_per_sec;
}

#endif  //  __COMMON_HLSLI__
