#pragma once

#include <cstdlib>
#include <cstring>

#include "Math/math_common.h"
#include "Math/vector2.h"
#include "Math/vector3.h"
#include "Math/vector4.h"

namespace math
{
	namespace detail
	{
		//	ベクトル型の判定と SIMD レジスタを持つかどうか
		template<typename V>
		struct vector_traits
		{
			static constexpr bool is_vector = false;
			static constexpr bool use_register = false;
		};
		template<typename T>
		struct vector_traits<vector2<T>>
		{
			static constexpr bool is_vector = true;
			static constexpr bool use_register = use_simd_v<T>;
		};
		template<typename T>
		struct vector_traits<vector3<T>>
		{
			static constexpr bool is_vector = true;
			static constexpr bool use_register = use_simd_v<T>;
		};
		template<typename T>
		struct vector_traits<vector4<T>>
		{
			static constexpr bool is_vector = true;
			static constexpr bool use_register = use_simd_v<T>;
		};
		template<typename V>
		constexpr bool is_vector_v = vector_traits<V>::is_vector;

		//	テンプレート引数の推論対象から外すための型
		template<typename T>
		struct identity { using type = T; };
		template<typename T>
		using identity_t = typename identity<T>::type;

		template<typename V>
		V abs_elements(const V& v)
		{
			if constexpr (vector_traits<V>::use_register)
			{
				return V(simd_ops_t<typename V::value_type>::abs(v.reg));
			}
			else
			{
				V ret = v;
				for (std::uint32_t i = 0; i < V::element_count; ++i)
				{
					ret[i] = std::abs(ret[i]);
				}
				return ret;
			}
		}

		template<typename V>
		V min_elements(const V& v, const V& min_value)
		{
			if constexpr (vector_traits<V>::use_register)
			{
				return V(simd_ops_t<typename V::value_type>::min(v.reg, min_value.reg));
			}
			else
			{
				V ret = v;
				for (std::uint32_t i = 0; i < V::element_count; ++i)
				{
					ret[i] = v[i] < min_value[i] ? v[i] : min_value[i];
				}
				return ret;
			}
		}

		template<typename V>
		V max_elements(const V& v, const V& max_value)
		{
			if constexpr (vector_traits<V>::use_register)
			{
				return V(simd_ops_t<typename V::value_type>::max(v.reg, max_value.reg));
			}
			else
			{
				V ret = v;
				for (std::uint32_t i = 0; i < V::element_count; ++i)
				{
					ret[i] = v[i] < max_value[i] ? max_value[i] : v[i];
				}
				return ret;
			}
		}

		template<typename V>
		V clamp_elements(const V& v, const V& min_value, const V& max_value)
		{
			return min_elements(max_elements(v, min_value), max_value);
		}

		template<typename V>
		V saturate_elements(const V& v)
		{
			V zero;
			V one;
			for (std::uint32_t i = 0; i < V::element_count; ++i)
			{
				zero[i] = 0;
				one[i] = 1;
			}
			return clamp_elements(v, zero, one);
		}

	}	//	namespace detail

	//	通常のsqrtよりも低精度だが、高速な根号計算を行う
	inline float approx_sqrt(float value)
	{
		float half = 0.5f * value;
		std::int32_t temp;
		std::memcpy(&temp, &value, sizeof(temp));
		temp = 0x5F3759DF - (temp >> 1);
		float result;
		std::memcpy(&result, &temp, sizeof(result));
		result *= (1.5f - (half * result * result));
	//	result *= (1.5f - (half * result * result));	//	コメントアウトを外すと精度が上がる
		return result * value;
	}

	//	value の絶対値を算出
	template<typename T, std::enable_if_t<std::is_arithmetic_v<T> && std::is_signed_v<T>, int> = 0>
	T abs(T value) { return std::abs(value); }
	template<typename V, std::enable_if_t<detail::is_vector_v<V>, int> = 0>
	V abs(const V& value) { return detail::abs_elements(value); }

	//	value と min_value を比較して小さい値を返す
	template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	constexpr T (min)(T value, T min_value) { return min_value < value ? min_value : value; }

	//	value と max_value を比較して大きい値を返す
	template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	constexpr T (max)(T value, T max_value) { return value < max_value ? max_value : value; }

	//	value を min_value ～ max_value 間に限定する
	template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	constexpr T clamp(T value, T min_value, T max_value) { return value > max_value ? max_value : (value < min_value ? min_value : value); }

	//	value を 0 ～ 1 間に限定する
	template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	constexpr T saturate(T value) { return clamp(value, static_cast<T>(0), static_cast<T>(1)); }

	//	value が min_value ～ max_value の内側にあるときは、近い方の端へ押し出す（外側ならそのまま）
	template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	constexpr T clamp_outside(T value, T min_value, T max_value)
	{
		if (value > min_value && value < max_value)
		{
			return (value - min_value < max_value - value) ? min_value : max_value;
		}
		return value;
	}

	//	value が edge 以上なら 1、未満なら 0（HLSL の step と同じ）
	template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	constexpr T step(T edge, T value) { return value >= edge ? static_cast<T>(1) : static_cast<T>(0); }

	//	value が min_value ～ max_value の範囲でどの位置にあるかを 0 ～ 1 で返す
	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	constexpr T linearstep(T min_value, T max_value, T value) { return saturate((value - min_value) / (max_value - min_value)); }

	//	linearstep をエルミート補間で滑らかにしたもの（HLSL の smoothstep と同じ）
	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	constexpr T smoothstep(T min_value, T max_value, T value)
	{
		const T t = linearstep(min_value, max_value, value);
		return t * t * (static_cast<T>(3) - (t + t));
	}

	//	線形補間（整数は補間後に切り捨てる）
	template<typename T, typename U, std::enable_if_t<std::is_arithmetic_v<T> && std::is_floating_point_v<U>, int> = 0>
	constexpr T lerp(T l, T r, U value) { return l + static_cast<T>(static_cast<U>(r - l) * value); }

	//	値の差が tolerance 以下か
	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	bool nearly_equal(T l, T r, T tolerance = constants<T>::epsilon * static_cast<T>(10)) { return std::abs(l - r) <= tolerance; }
	//	すべての要素の差が tolerance 以下か
	template<typename V, std::enable_if_t<detail::is_vector_v<V>, int> = 0>
	bool nearly_equal(const V& l, const V& r, typename V::value_type tolerance = constants<typename V::value_type>::epsilon * static_cast<typename V::value_type>(10))
	{
		for (std::uint32_t i = 0; i < V::element_count; ++i)
		{
			if (!(std::abs(l[i] - r[i]) <= tolerance))
			{
				return false;
			}
		}
		return true;
	}

	//	min_value ～ max_value の乱数を取得（std::rand を使う。分解能は 1/100000）
	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	T rand_range(T min_value, T max_value)
	{
		const T rate = static_cast<T>(std::rand() % 100000) * static_cast<T>(0.00001);
		return rate * (max_value - min_value) + min_value;
	}

	//	value を 2 のべき乗の alignment に切り上げる
	template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	constexpr T calc_alignment_power_of_two(T value, T alignment)
	{
		return (value + (alignment - 1)) & ~(alignment - 1);
	}

#define	MATH_DEFINE_VECTOR_UTILITY(vector_type)																													\
	/*	value と min_value を比較して小さい値を返す	*/																												\
	inline vector_type (min)(const vector_type& value, const vector_type& min_value) { return detail::min_elements(value, min_value); }								\
	/*	value と max_value を比較して大きい値を返す	*/																												\
	inline vector_type (max)(const vector_type& value, const vector_type& max_value) { return detail::max_elements(value, max_value); }								\
	/*	value を min_value ～ max_value 間に限定する	*/																											\
	inline vector_type clamp(const vector_type& value, const vector_type& min_value, const vector_type& max_value) { return detail::clamp_elements(value, min_value, max_value); }	\
	/*	value を 0 ～ 1 間に限定する	*/																															\
	inline vector_type saturate(const vector_type& value) { return detail::saturate_elements(value); }

	MATH_DEFINE_VECTOR_UTILITY(vector2f)
	MATH_DEFINE_VECTOR_UTILITY(vector2d)
	MATH_DEFINE_VECTOR_UTILITY(vector3f)
	MATH_DEFINE_VECTOR_UTILITY(vector3d)
	MATH_DEFINE_VECTOR_UTILITY(vector4f)
	MATH_DEFINE_VECTOR_UTILITY(vector4d)

#undef	MATH_DEFINE_VECTOR_UTILITY

	//	value を 0 ～ max_value 間でループさせる
	template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	T repeat(T max_value, T value)
	{
		T param;
		if constexpr (std::is_floating_point_v<T>)
		{
			param = std::fmod(value, max_value);
		}
		else
		{
			param = value % max_value;
		}
		return param < 0 ? max_value + param : param;
	}
	template<typename V, std::enable_if_t<detail::is_vector_v<V>, int> = 0>
	V repeat(const V& max_value, const V& value)
	{
		V ret;
		for (std::uint32_t i = 0; i < V::element_count; ++i)
		{
			ret[i] = repeat(max_value[i], value[i]);
		}
		return ret;
	}

	//	value を min_value ～ max_value 間でループさせる
	template<typename T>
	T wrap(const T& min_value, const T& max_value, const T& value)
	{
		return repeat(max_value - min_value, value - min_value) + min_value;
	}

	//	value を min_value ～ max_value 間で反復させる
	template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	T mirror(T min_value, T max_value, T value)
	{
		const T length = max_value - min_value;
		const T offset = value - min_value;
		std::int64_t count;
		T mod_value;
		if constexpr (std::is_floating_point_v<T>)
		{
			const T floor_count = std::floor(offset / length);
			count = static_cast<std::int64_t>(floor_count);
			mod_value = offset - floor_count * length;
		}
		else
		{
			count = static_cast<std::int64_t>(offset / length);
			mod_value = offset % length;
			if (mod_value < 0)
			{
				mod_value += length;
				--count;
			}
		}
		return count % 2 == 0 ? min_value + mod_value : max_value - mod_value;
	}
	template<typename V, std::enable_if_t<detail::is_vector_v<V>, int> = 0>
	V mirror(const V& min_value, const V& max_value, const V& value)
	{
		V ret;
		for (std::uint32_t i = 0; i < V::element_count; ++i)
		{
			ret[i] = mirror(min_value[i], max_value[i], value[i]);
		}
		return ret;
	}

	//	value が min_value ～ max_value 間にあるか
	template<typename T>
	bool between(T min_value, T max_value, T value)
	{
		return min_value <= value && value <= max_value;
	}

	//	value の要素から最も小さい値を取得する
	template<typename V, std::enable_if_t<detail::is_vector_v<V>, int> = 0>
	typename V::value_type element_min(const V& value)
	{
		typename V::value_type ret = value[0];
		for (std::uint32_t i = 1; i < V::element_count; ++i)
		{
			ret = value[i] < ret ? value[i] : ret;
		}
		return ret;
	}

	//	value の要素から最も大きい値を取得する
	template<typename V, std::enable_if_t<detail::is_vector_v<V>, int> = 0>
	typename V::value_type element_max(const V& value)
	{
		typename V::value_type ret = value[0];
		for (std::uint32_t i = 1; i < V::element_count; ++i)
		{
			ret = ret < value[i] ? value[i] : ret;
		}
		return ret;
	}

	//	value の符号を取得
	template<typename T>
	T sign(T value)
	{
		return static_cast<T>((value > static_cast<T>(0)) - (value < static_cast<T>(0)));
	}

	//	value の符号を取得。0は+1を返す
	template<typename T>
	T sign0plus(T value)
	{
		return static_cast<T>((value >= static_cast<T>(0)) - (value < static_cast<T>(0)));
	}

	//	value が負数か判定
	inline bool signbit(float value) { return std::signbit(value); }
	inline bool signbit(double value) { return std::signbit(value); }

	//	指定の数値よりも大きい2のべき乗数を取得
	inline std::int32_t round_up_to_power_of_two(std::int32_t value)
	{
		std::int32_t i = 1;
		for (; i < value; i *= 2);
		return i;
	}

	//	正規化済み深度から非線形深度へ変換
	template<typename T>
	T convert_depth(T ndc_depth, T near_z, T far_z, projection_z_order z_order = default_projection_z_order)
	{
		if (far_z > near_z)
		{
			if (z_order == projection_z_order::normal)
			{
				return near_z * far_z / (far_z - ndc_depth * (far_z - near_z));
			}
			//	Reverse
			return near_z * far_z / (near_z + ndc_depth * (far_z - near_z));
		}

		//	無限遠平面
		//	リバースデプスは ndc = near / z、通常の深度は ndc = 1 - near / z
		if (z_order == projection_z_order::normal)
		{
			return near_z / (static_cast<T>(1) - ndc_depth);
		}
		return near_z / ndc_depth;
	}

	//	正規化済み深度から線形深度へ変換
	template<typename T>
	T convert_linear_depth(T ndc_depth, T near_z, T far_z, projection_z_order z_order = default_projection_z_order)
	{
		if (far_z > near_z)
		{
			if (z_order == projection_z_order::normal)
			{
				T a = far_z / near_z;
				return ndc_depth / (a - ndc_depth * (a - static_cast<T>(1)));
			}
			//	Reverse
			return (static_cast<T>(1) - ndc_depth) / (static_cast<T>(1) + ndc_depth * (far_z / near_z - static_cast<T>(1)));
		}

		//	無限遠平面
		return near_z / ndc_depth;
	}

	//	秒からミリ秒へ
	template<typename T>
	constexpr T second_to_millisecond(T second) { return second * static_cast<T>(1000); }

	//	ミリ秒から秒へ
	template<typename T>
	constexpr T millisecond_to_second(T millisecond) { return millisecond / static_cast<T>(1000); }

	//	メートルからミリメートルへ
	template<typename T>
	constexpr T meter_to_millimeter(T meter) { return meter * static_cast<T>(1000); }

	//	ミリメートルからメートルへ
	template<typename T>
	constexpr T millimeter_to_meter(T millimeter) { return millimeter / static_cast<T>(1000); }

	//	次の二乗値を算出
	constexpr std::uint32_t calc_next_power_of_two(std::uint32_t value)
	{
		--value;
		value |= value >> 1;
		value |= value >> 2;
		value |= value >> 4;
		value |= value >> 8;
		value |= value >> 16;
		return ++value;
	}

	//	次の二乗値を算出
	constexpr std::uint64_t calc_next_power_of_two(std::uint64_t value)
	{
		--value;
		value |= value >> 1;
		value |= value >> 2;
		value |= value >> 4;
		value |= value >> 8;
		value |= value >> 16;
		value |= value >> 32;
		return ++value;
	}

	//	指定のアライメント計算
	constexpr std::uint64_t calc_alignment(std::uint64_t value, std::uint64_t alignment)
	{
		return ((value + alignment - 1) / alignment) * alignment;
	}

	//	桁数の計算
	constexpr std::uint64_t calc_digit(std::uint64_t value, bool counting_zeros = false)
	{
		if (value == 0 && counting_zeros)
		{
			return 1;
		}

		std::uint64_t digit = 0;
		while (value != 0)
		{
			value /= 10;
			++digit;
		}
		return digit;
	}

	//	エンディアン変換
	//	用途が少ないので一部しか対応していない
	//	必要に応じて追加
	constexpr std::int8_t convert_endian(std::int8_t value) { return value; }
	constexpr std::uint8_t convert_endian(std::uint8_t value) { return value; }
	constexpr std::uint16_t convert_endian(std::uint16_t value)
	{
		return static_cast<std::uint16_t>((value >> 8) | (value << 8));
	}
	constexpr std::uint32_t convert_endian(std::uint32_t value)
	{
		return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) | ((value & 0x00FF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
	}
	constexpr std::uint64_t convert_endian(std::uint64_t value)
	{
		return (static_cast<std::uint64_t>(convert_endian(static_cast<std::uint32_t>(value))) << 32) | convert_endian(static_cast<std::uint32_t>(value >> 32));
	}
	constexpr std::int16_t convert_endian(std::int16_t value) { return static_cast<std::int16_t>(convert_endian(static_cast<std::uint16_t>(value))); }
	constexpr std::int32_t convert_endian(std::int32_t value) { return static_cast<std::int32_t>(convert_endian(static_cast<std::uint32_t>(value))); }
	constexpr std::int64_t convert_endian(std::int64_t value) { return static_cast<std::int64_t>(convert_endian(static_cast<std::uint64_t>(value))); }

	//	三角形の面積を算出する
	template<typename T>
	T calculate_triangle_area(const vector2<T>& vertex1, const vector2<T>& vertex2, const vector2<T>& vertex3)
	{
		T a = (vertex2 - vertex1).length();
		T b = (vertex3 - vertex2).length();
		T c = (vertex1 - vertex3).length();
		T s = (a + b + c) * static_cast<T>(0.5);
		return std::sqrt(s * (s - a) * (s - b) * (s - c));
	}
	template<typename T>
	T calculate_triangle_area(const vector3<T>& vertex1, const vector3<T>& vertex2, const vector3<T>& vertex3)
	{
		T a = (vertex2 - vertex1).length();
		T b = (vertex3 - vertex2).length();
		T c = (vertex1 - vertex3).length();
		T s = (a + b + c) * static_cast<T>(0.5);
		return std::sqrt(s * (s - a) * (s - b) * (s - c));
	}

}	//	namespace math
