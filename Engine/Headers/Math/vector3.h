#pragma once

#include "Math/math_common.h"

namespace math
{
	//	3次元ベクトル演算クラス
	//	@tool SIMD 使用時は 4 要素分の領域を持ち、w は常に 0 を保つ
	//	@tool サイズ：float 版 16 バイト、double 版 32 バイト（32 バイト境界。MATH_DISABLE_SIMD 時は 12 / 24 バイト）
	template<typename T>
	class alignas(simd_alignment_v<T>) vector3
	{
		static_assert(is_supported_scalar_v<T>, "vector3 は float / double のみ対応");

	public:
		using value_type = T;
		using ops = simd_ops_t<T>;
		using register_type = simd_register_t<T>;
		static constexpr std::uint32_t element_count = 3;	//	要素数
		//	@tool SIMD 使用時はレジスタ幅分の領域を確保する
		static constexpr std::uint32_t storage_count = use_simd_v<T> ? 4 : 3;

	public:
		static vector3 zero() { return vector3(0, 0, 0); }
		static vector3 x_axis() { return vector3(1, 0, 0); }
		static vector3 y_axis() { return vector3(0, 1, 0); }
		static vector3 z_axis() { return vector3(0, 0, 1); }
		static vector3 axis(std::int32_t index) { vector3 ret(0, 0, 0); ret[index] = static_cast<T>(1); return ret; }

	public:
		vector3() { set(0, 0, 0); }
		vector3(const vector3&) = default;
		vector3& operator=(const vector3&) = default;
		vector3(T x, T y, T z) { set(x, y, z); }
		explicit vector3(const T values[element_count]) { set(values); }
		explicit vector3(const vector2<T>& v, T z = 0);
		explicit vector3(const vector4<T>& v);
		vector3(const DirectX::XMFLOAT3& v) { set(static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z)); }
		//	@tool float <> double の変換（精度が変わるため明示的に行う）
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit vector3(const vector3<U>& v) { set(static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z)); }
		//	@tool SIMD レジスタから生成（w は 0 にする）
		explicit vector3(const register_type& v) { set(v); }

		void set(T value) { set(value, value, value); }
		void set(const T values[element_count]) { set(values[0], values[1], values[2]); }
		void set(T set_x, T set_y, T set_z)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::set(set_x, set_y, set_z, 0);
			}
			else
			{
				x = set_x;
				y = set_y;
				z = set_z;
			}
		}
		void set(const register_type& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::mask_xyz(v);
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
				z = -z;
			}
		}

		//	長さ取得
		T length() const
		{
			if constexpr (use_simd_v<T>)
			{
				return ops::first(ops::sqrt(ops::dot3(reg, reg)));
			}
			else
			{
				return std::sqrt(x * x + y * y + z * z);
			}
		}

		//	長さの2乗を取得
		T square_length() const { return dot(*this, *this); }

		//	正規化
		void normalize()
		{
			if constexpr (use_simd_v<T>)
			{
				reg = simd::normalize<ops>(reg, ops::dot3(reg, reg), constants<T>::normalize_threshold);
			}
			else
			{
				T work = length();
				work = (work >= constants<T>::normalize_threshold) ? static_cast<T>(1) / work : static_cast<T>(0);
				x *= work;
				y *= work;
				z *= work;
			}
		}

		//	正規化済みベクトルを取得
		vector3 create_normalize() const { vector3 ret = *this; ret.normalize(); return ret; }

	public:
		//	足し算
		void addition(const vector3& v)
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
			}
		}

		//	引き算
		void subtract(const vector3& v)
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
			}
		}

		//	掛け算
		void multiply(const vector3& v)
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
			}
		}

		//	@tool スカラー倍
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
			}
		}

		//	割り算
		//	@tool SIMD 版は w = 0 / 0 にならないよう xyz のみ割って w を 0 に戻す
		void division(const vector3& v)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::mask_xyz(ops::div(reg, v.reg));
			}
			else
			{
				x /= v.x;
				y /= v.y;
				z /= v.z;
			}
		}

		//	@tool スカラー割り
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
			}
		}

		//	クォータニオン * ベクトル * 逆クォータニオン の結果を算出する
		void multiply(const quaternion<T>& q);

		//	内積
		T dot(const vector3& v) const { return dot(*this, v); }

		//	外積
		vector3 cross(const vector3& v) const { return cross(*this, v); }

		//	指定のベクトルを指定の行列空間に変換したベクトルを取得
		vector4<T> create_transform(const matrix<T>& m) const;

		//	指定のベクトルを指定の行列空間に変換し、結果を w = 1 に投影する
		void transform_coord(const matrix<T>& m);

		//	指定のベクトルを指定の行列空間に変換し、結果を w = 1 に投影したベクトルを取得
		vector3 create_transform_coord(const matrix<T>& m) const;

		//	指定のベクトルを指定の行列空間に変換し、結果を w = 0 に投影する
		void transform_normal(const matrix<T>& m);

		//	指定のベクトルを指定の行列空間に変換し、結果を w = 0 に投影したベクトルを取得
		vector3 create_transform_normal(const matrix<T>& m) const;

		//	@tool DirectXMath との相互変換（double 版は float に丸められる）
		DirectX::XMFLOAT3 to_xmfloat3() const { return DirectX::XMFLOAT3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }
		DirectX::XMVECTOR to_xmvector() const
		{
			if constexpr (use_simd_v<T>)
			{
				return ops::to_float4(reg);
			}
			else
			{
				return DirectX::XMVectorSet(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 0.0f);
			}
		}
		static vector3 load(DirectX::FXMVECTOR v)
		{
			if constexpr (use_simd_v<T>)
			{
				return vector3(ops::from_float4(v));
			}
			else
			{
				DirectX::XMFLOAT3 f;
				DirectX::XMStoreFloat3(&f, v);
				return vector3(f);
			}
		}

	public:
		T& operator[](std::int32_t index) { return elements[index]; }
		const T& operator[](std::int32_t index) const { return elements[index]; }

		vector3 operator+(const vector3& v) const { vector3 ret = *this; ret.addition(v); return ret; }
		vector3& operator+=(const vector3& v) { addition(v); return *this; }
		vector3 operator+() const { return *this; }

		vector3 operator-(const vector3& v) const { vector3 ret = *this; ret.subtract(v); return ret; }
		vector3& operator-=(const vector3& v) { subtract(v); return *this; }
		vector3 operator-() const { vector3 ret = *this; ret.negate(); return ret; }

		vector3 operator*(const vector3& v) const { vector3 ret = *this; ret.multiply(v); return ret; }
		vector3 operator*(T value) const { vector3 ret = *this; ret.multiply(value); return ret; }
		vector3& operator*=(const vector3& v) { multiply(v); return *this; }
		vector3& operator*=(T value) { multiply(value); return *this; }

		vector3 operator/(const vector3& v) const { vector3 ret = *this; ret.division(v); return ret; }
		vector3 operator/(T value) const { vector3 ret = *this; ret.division(value); return ret; }
		vector3& operator/=(const vector3& v) { division(v); return *this; }
		vector3& operator/=(T value) { division(value); return *this; }

		bool operator==(const vector3& v) const { return x == v.x && y == v.y && z == v.z; }
		bool operator!=(const vector3& v) const { return !(*this == v); }

	public:
		//	内積
		static T dot(const vector3& l, const vector3& r)
		{
			if constexpr (use_simd_v<T>)
			{
				return ops::first(ops::dot3(l.reg, r.reg));
			}
			else
			{
				return l.x * r.x + l.y * r.y + l.z * r.z;
			}
		}

		//	外積
		static vector3 cross(const vector3& l, const vector3& r)
		{
			if constexpr (use_simd_v<T>)
			{
				return vector3(simd::cross3<ops>(l.reg, r.reg));
			}
			else
			{
				return vector3(l.y * r.z - l.z * r.y,
							   l.z * r.x - l.x * r.z,
							   l.x * r.y - l.y * r.x);
			}
		}

		//	平面に擦るようなベクトルを算出
		static vector3 scratch(const vector3& front, const vector3& normal) { return front - normal * dot(front, normal); }

		//	法線に対して反射させたベクトルを算出
		//	@tool Framework のスカラー版は 2 倍を重ねて掛けていた（4 倍になる）ため SIMD 版の式に合わせている
		static vector3 reflect(const vector3& front, const vector3& normal) { return front - normal * (static_cast<T>(2) * dot(front, normal)); }

		//	指定のベクトルを指定の行列空間に変換する
		static vector4<T> transform(const vector3& l, const matrix<T>& r);

		//	指定のベクトルを指定の行列空間に変換し、結果を w = 1 に投影する
		//	主に座標に使用
		static vector3 transform_coord(const vector3& l, const matrix<T>& r);

		//	指定のベクトルを指定の行列空間に変換し、結果を w = 0 に投影する
		//	主にベクトルに使用
		static vector3 transform_normal(const vector3& l, const matrix<T>& r);

		//	スクリーン座標から射影座標へ変換する
		static vector3 calc_screen_to_projection(T screen_x, T screen_y, T depth, const matrix<T>& projection, const matrix<T>& viewport);

		//	スクリーン座標からワールド座標へ変換する
		static vector3 calc_screen_to_world(T screen_x, T screen_y, T depth, const matrix<T>& view, const matrix<T>& projection, const matrix<T>& viewport);

	public:
		union
		{
			register_type reg;
			T elements[storage_count];
			struct { T x, y, z; };
		};
	};

	template<typename T>
	inline vector3<T> operator*(T value, const vector3<T>& v) { return v * value; }

	using vector3f = vector3<float>;
	using vector3d = vector3<double>;

	extern template class vector3<float>;
	extern template class vector3<double>;

}	//	namespace math
