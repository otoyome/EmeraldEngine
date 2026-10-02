#pragma once

#include "Math/math_common.h"

namespace math
{
	//	4次元ベクトル演算クラス
	//	@tool 【格納】x, y, z, w。規約に関係なく同じで、内部の計算も 1 通り
	//	@tool 【規約で変わるもの】行列との積の書き方（行ベクトル規約：v * M、列ベクトル規約：M * v）
	template<typename T>
	class alignas(simd_alignment_v<T>) vector4
	{
		static_assert(is_supported_scalar_v<T>, "vector4 は float / double のみ対応");

	public:
		using value_type = T;
		using ops = simd_ops_t<T>;
		using register_type = simd_register_t<T>;
		static constexpr std::uint32_t element_count = 4;	//	要素数

	public:
		static vector4 zero() { return vector4(0, 0, 0, 0); }
		static vector4 x_axis() { return vector4(1, 0, 0, 0); }
		static vector4 y_axis() { return vector4(0, 1, 0, 0); }
		static vector4 z_axis() { return vector4(0, 0, 1, 0); }
		static vector4 w_axis() { return vector4(0, 0, 0, 1); }

	public:
		vector4() { set(0, 0, 0, 0); }
		vector4(const vector4&) = default;
		vector4& operator=(const vector4&) = default;
		vector4(T x, T y, T z, T w) { set(x, y, z, w); }
		explicit vector4(const T values[element_count]) { set(values); }
		explicit vector4(const vector2<T>& v);
		vector4(const vector2<T>& v1, const vector2<T>& v2);
		explicit vector4(const vector3<T>& v);
		vector4(const vector3<T>& v, T w);
		vector4(const basic_element4<T>& v) { set(v.x, v.y, v.z, v.w); }
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit vector4(const basic_element4<U>& v) { set(static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z), static_cast<T>(v.w)); }
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit vector4(const vector4<U>& v) { set(static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z), static_cast<T>(v.w)); }
		explicit vector4(const register_type& v) { reg = v; }

		void set(T value) { set(value, value, value, value); }
		void set(const T values[element_count]) { set(values[0], values[1], values[2], values[3]); }
		void set(T set_x, T set_y, T set_z, T set_w)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::set(set_x, set_y, set_z, set_w);
			}
			else
			{
				x = set_x;
				y = set_y;
				z = set_z;
				w = set_w;
			}
		}
		void set(const register_type& v) { reg = v; }

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
				z = -z;
				w = -w;
			}
		}

		//	長さ取得
		T length() const
		{
			if constexpr (use_simd_v<T>)
			{
				return ops::first(ops::sqrt(ops::dot4(reg, reg)));
			}
			else
			{
				return std::sqrt(x * x + y * y + z * z + w * w);
			}
		}

		//	長さの2乗を取得
		T square_length() const { return dot(*this, *this); }

		//	正規化
		void normalize()
		{
			if constexpr (use_simd_v<T>)
			{
				reg = simd::normalize<ops>(reg, ops::dot4(reg, reg), constants<T>::normalize_threshold);
			}
			else
			{
				T work = length();
				work = (work >= constants<T>::normalize_threshold) ? static_cast<T>(1) / work : static_cast<T>(0);
				x *= work;
				y *= work;
				z *= work;
				w *= work;
			}
		}

		//	正規化済みベクトルを取得
		vector4 normalized() const { vector4 ret = *this; ret.normalize(); return ret; }

		//	Vector3 に変換して取得
		vector3<T> to_vector3() const;

	public:
		//	足し算
		void addition(const vector4& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::add(reg, v.reg);
			}
			else
			{
				x += v.x;
				y += v.y;
				z += v.z;
				w += v.w;
			}
		}

		//	引き算
		void subtract(const vector4& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::sub(reg, v.reg);
			}
			else
			{
				x -= v.x;
				y -= v.y;
				z -= v.z;
				w -= v.w;
			}
		}

		//	掛け算
		void multiply(const vector4& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::mul(reg, v.reg);
			}
			else
			{
				x *= v.x;
				y *= v.y;
				z *= v.z;
				w *= v.w;
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
				z *= value;
				w *= value;
			}
		}

		//	割り算
		void division(const vector4& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::div(reg, v.reg);
			}
			else
			{
				x /= v.x;
				y /= v.y;
				z /= v.z;
				w /= v.w;
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
				z /= value;
				w /= value;
			}
		}

		//	内積
		T dot(const vector4& v) const { return dot(*this, v); }

		//	外積
		vector4 cross(const vector4& center, const vector4& right) const { return cross(*this, center, right); }

		//	型変換
		float4 to_float4() const
		{
			float4 ret;
			if constexpr (use_simd_v<T>)
			{
				_mm_storeu_ps(ret.elements, ops::to_float4(reg));
			}
			else
			{
				ret = float4(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w));
			}
			return ret;
		}
		double4 to_double4() const { return double4(static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(w)); }

	public:
		T& operator[](std::int32_t index) { return elements[index]; }
		const T& operator[](std::int32_t index) const { return elements[index]; }

		vector4 operator+(const vector4& v) const { vector4 ret = *this; ret.addition(v); return ret; }
		vector4& operator+=(const vector4& v) { addition(v); return *this; }
		vector4 operator+() const { return *this; }

		vector4 operator-(const vector4& v) const { vector4 ret = *this; ret.subtract(v); return ret; }
		vector4& operator-=(const vector4& v) { subtract(v); return *this; }
		vector4 operator-() const { vector4 ret = *this; ret.negate(); return ret; }

		vector4 operator*(const vector4& v) const { vector4 ret = *this; ret.multiply(v); return ret; }
		vector4 operator*(T value) const { vector4 ret = *this; ret.multiply(value); return ret; }
		vector4& operator*=(const vector4& v) { multiply(v); return *this; }
		vector4& operator*=(T value) { multiply(value); return *this; }

		vector4 operator/(const vector4& v) const { vector4 ret = *this; ret.division(v); return ret; }
		vector4 operator/(T value) const { vector4 ret = *this; ret.division(value); return ret; }
		vector4& operator/=(const vector4& v) { division(v); return *this; }
		vector4& operator/=(T value) { division(value); return *this; }

		bool operator==(const vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
		bool operator!=(const vector4& v) const { return !(*this == v); }

	public:
		//	内積
		static T dot(const vector4& l, const vector4& r)
		{
			if constexpr (use_simd_v<T>)
			{
				return ops::first(ops::dot4(l.reg, r.reg));
			}
			else
			{
				return l.x * r.x + l.y * r.y + l.z * r.z + l.w * r.w;
			}
		}

		//	外積
		static vector4 cross(const vector4& l, const vector4& center, const vector4& r);

		//	指定のベクトルを指定の行列空間に変換する
		static vector4 transform(const vector3<T>& l, const matrix<T>& r);

		//	指定のベクトルを指定の行列空間に変換する
		static vector4 transform(const vector4& l, const matrix<T>& r);

	public:
		union
		{
			register_type reg;
			T elements[element_count];
			struct { T x, y, z, w; };
		};
	};

	template<typename T>
	inline vector4<T> operator*(T value, const vector4<T>& v) { return v * value; }

	using vector4f = vector4<float>;
	using vector4d = vector4<double>;

	extern template class vector4<float>;
	extern template class vector4<double>;

}	//	namespace math
