#pragma once

#include "Math/math_common.h"

namespace math
{
	//	2次元ベクトル
	template<typename T>
	class alignas(simd_alignment_v<T>) vector2
	{
		static_assert(is_supported_scalar_v<T>, "vector2 は float / double のみ対応");

	public:
		using value_type = T;
		using ops = simd_ops_t<T>;
		using register_type = simd_register_t<T>;
		static constexpr std::uint32_t element_count = 2;	//	要素数
		static constexpr std::uint32_t storage_count = use_simd_v<T> ? 4 : 2;

	public:
		static vector2 zero() { return vector2(0, 0); }
		static vector2 x_axis() { return vector2(1, 0); }
		static vector2 y_axis() { return vector2(0, 1); }

	public:
		vector2() { set(0, 0); }
		vector2(const vector2&) = default;
		vector2& operator=(const vector2&) = default;
		vector2(T x, T y) { set(x, y); }
		explicit vector2(const T values[element_count]) { set(values[0], values[1]); }
		explicit vector2(const vector3<T>& v);
		explicit vector2(const vector4<T>& v);
		vector2(const basic_element2<T>& v) { set(v.x, v.y); }
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit vector2(const basic_element2<U>& v) { set(static_cast<T>(v.x), static_cast<T>(v.y)); }
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit vector2(const vector2<U>& v) { set(static_cast<T>(v.x), static_cast<T>(v.y)); }
		explicit vector2(const register_type& v) { set(v); }

		void set(T value) { set(value, value); }
		void set(const T values[element_count]) { set(values[0], values[1]); }
		void set(T set_x, T set_y)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::set(set_x, set_y, 0, 0);
			}
			else
			{
				x = set_x;
				y = set_y;
			}
		}
		void set(const register_type& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::mask_xy(v);
			}
			else
			{
				reg = v;
			}
		}

		//	反転
		void negate()
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::neg(reg);
			}
			else
			{
				x = -x;
				y = -y;
			}
		}

		//	長さ取得
		T length() const
		{
			if constexpr (use_simd_v<T>)
			{
				return ops::first(ops::sqrt(ops::dot2(reg, reg)));
			}
			else
			{
				return std::sqrt(x * x + y * y);
			}
		}

		//	長さの2乗を取得
		T square_length() const { return dot(*this, *this); }

		//	正規化
		void normalize()
		{
			if constexpr (use_simd_v<T>)
			{
				reg = simd::normalize<ops>(reg, ops::dot2(reg, reg), constants<T>::normalize_threshold);
			}
			else
			{
				T work = length();
				work = (work >= constants<T>::normalize_threshold) ? static_cast<T>(1) / work : static_cast<T>(0);
				x *= work;
				y *= work;
			}
		}

		//	正規化済みベクトルを取得
		vector2 normalized() const { vector2 ret = *this; ret.normalize(); return ret; }

	public:
		//	足し算
		void addition(const vector2& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::add(reg, v.reg);
			}
			else
			{
				x += v.x;
				y += v.y;
			}
		}

		//	引き算
		void subtract(const vector2& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::sub(reg, v.reg);
			}
			else
			{
				x -= v.x;
				y -= v.y;
			}
		}

		//	掛け算
		void multiply(const vector2& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::mul(reg, v.reg);
			}
			else
			{
				x *= v.x;
				y *= v.y;
			}
		}

		//	スカラー倍
		void multiply(T value)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::mul(reg, ops::set1(value));
			}
			else
			{
				x *= value;
				y *= value;
			}
		}

		//	割り算
		void division(const vector2& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::mask_xy(ops::div(reg, v.reg));
			}
			else
			{
				x /= v.x;
				y /= v.y;
			}
		}

		//	スカラー割り
		void division(T value)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::div(reg, ops::set1(value));
			}
			else
			{
				x /= value;
				y /= value;
			}
		}

		//	内積
		T dot(const vector2& v) const { return dot(*this, v); }
		//	外積
		T cross(const vector2& v) const { return cross(*this, v); }

		//	行列と演算したベクトルを取得
		vector4<T> transformed(const matrix<T>& m) const;
		//	指定のベクトルを指定の行列空間に変換し、結果を w = 1 に投影する
		void transform_coord(const matrix<T>& m);
		//	指定のベクトルを指定の行列空間に変換し、結果を w = 1 に投影したベクトルを取得
		vector2 transformed_coord(const matrix<T>& m) const;

		//	型変換
		float2 to_float2() const { return float2(static_cast<float>(x), static_cast<float>(y)); }
		double2 to_double2() const { return double2(static_cast<double>(x), static_cast<double>(y)); }

	public:
		T& operator[](std::int32_t index) { return elements[index]; }
		const T& operator[](std::int32_t index) const { return elements[index]; }

		vector2 operator+(const vector2& v) const { vector2 ret = *this; ret.addition(v); return ret; }
		vector2& operator+=(const vector2& v) { addition(v); return *this; }
		vector2 operator+() const { return *this; }

		vector2 operator-(const vector2& v) const { vector2 ret = *this; ret.subtract(v); return ret; }
		vector2& operator-=(const vector2& v) { subtract(v); return *this; }
		vector2 operator-() const { vector2 ret = *this; ret.negate(); return ret; }

		vector2 operator*(const vector2& v) const { vector2 ret = *this; ret.multiply(v); return ret; }
		vector2 operator*(T value) const { vector2 ret = *this; ret.multiply(value); return ret; }
		vector2& operator*=(const vector2& v) { multiply(v); return *this; }
		vector2& operator*=(T value) { multiply(value); return *this; }

		vector2 operator/(const vector2& v) const { vector2 ret = *this; ret.division(v); return ret; }
		vector2 operator/(T value) const { vector2 ret = *this; ret.division(value); return ret; }
		vector2& operator/=(const vector2& v) { division(v); return *this; }
		vector2& operator/=(T value) { division(value); return *this; }

		bool operator==(const vector2& v) const { return x == v.x && y == v.y; }
		bool operator!=(const vector2& v) const { return !(*this == v); }

	public:
		//	内積
		static T dot(const vector2& l, const vector2& r)
		{
			if constexpr (use_simd_v<T>)
			{
				return ops::first(ops::dot2(l.reg, r.reg));
			}
			else
			{
				return l.x * r.x + l.y * r.y;
			}
		}

		//	外積
		static T cross(const vector2& l, const vector2& r) { return l.x * r.y - l.y * r.x; }

		//	指定のベクトルを指定の行列空間に変換する
		static vector4<T> transform(const vector2& l, const matrix<T>& r);

		//	指定のベクトルを指定の行列空間に変換し、結果を w = 1 に投影する
		static vector2 transform_coord(const vector2& l, const matrix<T>& r);

	public:
		union
		{
			register_type reg;
			T elements[storage_count];
			struct { T x, y; };
		};
	};

	template<typename T>
	inline vector2<T> operator*(T value, const vector2<T>& v) { return v * value; }

	using vector2f = vector2<float>;
	using vector2d = vector2<double>;

	extern template class vector2<float>;
	extern template class vector2<double>;

}	//	namespace math
