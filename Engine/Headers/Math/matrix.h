#pragma once

#include "Math/math_common.h"

namespace math
{
	//	4x4行列演算クラス
	//	@tool 行優先・行ベクトル（v * M）。平行移動は _41, _42, _43 に入る（DirectXMath と同じ）
	template<typename T>
	class alignas(simd_alignment_v<T>) matrix
	{
		static_assert(is_supported_scalar_v<T>, "matrix は float / double のみ対応");

	public:
		using value_type = T;
		using ops = simd_ops_t<T>;
		using register_type = simd_register_t<T>;
		static constexpr std::uint32_t element_count = 16;	//	要素数

	public:
		matrix() { identity(); }
		matrix(const matrix&) = default;
		matrix& operator=(const matrix&) = default;
		matrix(T m11, T m12, T m13, T m14,
			   T m21, T m22, T m23, T m24,
			   T m31, T m32, T m33, T m34,
			   T m41, T m42, T m43, T m44)
		{
			set(m11, m12, m13, m14,
				m21, m22, m23, m24,
				m31, m32, m33, m34,
				m41, m42, m43, m44);
		}
		matrix(const vector4<T>& v1, const vector4<T>& v2, const vector4<T>& v3, const vector4<T>& v4);
		matrix(const DirectX::XMFLOAT4X4& source);
		//	@tool float <> double の変換（精度が変わるため明示的に行う）
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit matrix(const matrix<U>& mat)
		{
			for (std::uint32_t i = 0; i < element_count; ++i)
			{
				elements[i] = static_cast<T>(mat.elements[i]);
			}
		}
		//	@tool SIMD レジスタから生成
		matrix(const register_type& line0, const register_type& line1, const register_type& line2, const register_type& line3)
		{
			reg[0] = line0;
			reg[1] = line1;
			reg[2] = line2;
			reg[3] = line3;
		}

		//	回転行列として生成
		explicit matrix(const quaternion<T>& rotation);

		//	姿勢行列として生成
		matrix(const quaternion<T>& rotation, const vector3<T>& position);

		void set(T value);
		void set(T m11, T m12, T m13, T m14,
				 T m21, T m22, T m23, T m24,
				 T m31, T m32, T m33, T m34,
				 T m41, T m42, T m43, T m44)
		{
			if constexpr (use_simd_v<T>)
			{
				reg[0] = ops::set(m11, m12, m13, m14);
				reg[1] = ops::set(m21, m22, m23, m24);
				reg[2] = ops::set(m31, m32, m33, m34);
				reg[3] = ops::set(m41, m42, m43, m44);
			}
			else
			{
				_11 = m11; _12 = m12; _13 = m13; _14 = m14;
				_21 = m21; _22 = m22; _23 = m23; _24 = m24;
				_31 = m31; _32 = m32; _33 = m33; _34 = m34;
				_41 = m41; _42 = m42; _43 = m43; _44 = m44;
			}
		}
		void set(const vector4<T>& v1, const vector4<T>& v2, const vector4<T>& v3, const vector4<T>& v4);

		//	転置する
		void transpose() { *this = transpose(*this); }

		//	転置済み行列を返す
		matrix create_transpose() const { return transpose(*this); }

		//	逆行列化
		void inverse() { *this = inverse(*this); }

		//	逆行列化済み行列を取得
		matrix create_inverse() const { return inverse(*this); }

		//	単位行列化
		void identity()
		{
			set(1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1);
		}

		//	S(Scale)R(Rotation)T(Translation)行列分解して各姿勢値を取得
		bool decompose(vector3<T>& out_scale, quaternion<T>& out_rotation, vector3<T>& out_position) const;

		//	行列式
		T determinant() const { return determinant(*this); }

		//	回転行列対象のミラーリング
		void mirroring(mirroring_order order);

		//	回転行列対象のミラーリング
		matrix create_mirroring(mirroring_order order) const { matrix ret = *this; ret.mirroring(order); return ret; }

		//	Vector4で要素を取得
		//	@tool index は行番号
		vector4<T> to_vector4(std::int32_t index) const;

		//	Vector3で要素を取得
		//	@tool index は行番号
		vector3<T> to_vector3(std::int32_t index) const;

		//	@tool 平行移動成分の取得 / 設定
		vector3<T> get_translation() const;
		void set_translation(const vector3<T>& position);

	public:
		//	足し算
		void addition(const matrix& r)
		{
			if constexpr (use_simd_v<T>)
			{
				for (std::int32_t i = 0; i < 4; ++i)
				{
					reg[i] = ops::add(reg[i], r.reg[i]);
				}
			}
			else
			{
				for (std::uint32_t i = 0; i < element_count; ++i)
				{
					elements[i] += r.elements[i];
				}
			}
		}

		//	引き算
		void subtract(const matrix& r)
		{
			if constexpr (use_simd_v<T>)
			{
				for (std::int32_t i = 0; i < 4; ++i)
				{
					reg[i] = ops::sub(reg[i], r.reg[i]);
				}
			}
			else
			{
				for (std::uint32_t i = 0; i < element_count; ++i)
				{
					elements[i] -= r.elements[i];
				}
			}
		}

		//	掛け算
		void multiply(T value)
		{
			if constexpr (use_simd_v<T>)
			{
				register_type v = ops::set1(value);
				for (std::int32_t i = 0; i < 4; ++i)
				{
					reg[i] = ops::mul(reg[i], v);
				}
			}
			else
			{
				for (std::uint32_t i = 0; i < element_count; ++i)
				{
					elements[i] *= value;
				}
			}
		}

		//	掛け算
		void multiply(const matrix& r)
		{
			if constexpr (use_simd_v<T>)
			{
				simd::multiply<ops>(reg, r.reg, reg);
			}
			else
			{
				matrix ret;
				for (std::int32_t row = 0; row < 4; ++row)
				{
					for (std::int32_t column = 0; column < 4; ++column)
					{
						ret.m[row][column] = m[row][0] * r.m[0][column]
										   + m[row][1] * r.m[1][column]
										   + m[row][2] * r.m[2][column]
										   + m[row][3] * r.m[3][column];
					}
				}
				*this = ret;
			}
		}

		//	@tool DirectXMath との相互変換（double 版は float に丸められる）
		DirectX::XMFLOAT4X4 to_xmfloat4x4() const;
		DirectX::XMMATRIX to_xmmatrix() const;
		static matrix load(DirectX::FXMMATRIX source);

	public:
		//	@tool 0～15 の通し番号でアクセスする
		T& operator[](std::int32_t index) { return elements[index]; }
		const T& operator[](std::int32_t index) const { return elements[index]; }
		matrix operator+() const { return *this; }
		matrix operator+(const matrix& r) const { matrix ret = *this; ret.addition(r); return ret; }
		matrix& operator+=(const matrix& r) { addition(r); return *this; }
		matrix operator-() const { return *this * static_cast<T>(-1); }
		matrix operator-(const matrix& r) const { matrix ret = *this; ret.subtract(r); return ret; }
		matrix& operator-=(const matrix& r) { subtract(r); return *this; }
		matrix operator*(const matrix& r) const { matrix ret = *this; ret.multiply(r); return ret; }
		matrix operator*(T value) const { matrix ret = *this; ret.multiply(value); return ret; }
		matrix& operator*=(const matrix& r) { multiply(r); return *this; }
		matrix& operator*=(T value) { multiply(value); return *this; }
		matrix operator/(T value) const { matrix ret = *this; ret.multiply(static_cast<T>(1) / value); return ret; }
		matrix& operator/=(T value) { multiply(static_cast<T>(1) / value); return *this; }

		bool operator==(const matrix& r) const
		{
			for (std::uint32_t i = 0; i < element_count; ++i)
			{
				if (elements[i] != r.elements[i])
				{
					return false;
				}
			}
			return true;
		}
		bool operator!=(const matrix& r) const { return !(*this == r); }

	public:
		//	行列式
		static T determinant(const matrix& mat);

		//	逆行列化
		//	@tool 行列式が 0 の場合は元の行列をそのまま返す（Framework の実装どおり）
		static matrix inverse(const matrix& mat);

		//	拡縮行列を対象とした逆行列化
		static matrix inverse_for_scaling(const matrix& mat);

		//	回転行列を対象とした逆行列化
		static matrix inverse_for_rotation(const matrix& mat);

		//	平行移動行列を対象とした逆行列化
		static matrix inverse_for_translation(const matrix& mat);

		//	拡縮しない姿勢行列を対象とした逆行列化
		static matrix inverse_for_rt(const matrix& mat);

		//	転置
		static matrix transpose(const matrix& mat);

		//	拡大縮小
		static matrix scaling(const vector3<T>& scale);

		//	拡大縮小
		static matrix scaling(T scale_x, T scale_y, T scale_z);

		//	ピッチ角を指定した回転行列を生成
		static matrix rotation_x(T pitch);

		//	ヨー角を指定した回転行列を生成
		static matrix rotation_y(T yaw);

		//	ロール角を指定した回転行列を生成
		static matrix rotation_z(T roll);

		//	オイラー角指定の回転行列を生成
		static matrix rotation_xyz(const vector3<T>& rotation, rotation_order order = default_rotation_order);

		//	オイラー角指定の回転行列を生成
		static matrix rotation_xyz(T pitch, T yaw, T roll, rotation_order order = default_rotation_order);

		//	オイラー角を取得する
		//	@return	ジンバルロック判定
		static bool to_euler_angle(vector3<T>& out, const matrix& rotation, rotation_order order = default_rotation_order);

		//	オイラー角を取得する
		//	@return	ジンバルロック判定
		static bool to_euler_angle(T& out_x, T& out_y, T& out_z, const matrix& rotation, rotation_order order = default_rotation_order);

		//	指定軸と軸周りの回転角度から回転行列を生成
		static matrix rotation_axis(const vector3<T>& axis, T angle);

		//	クォータニオンから回転行列を生成
		static matrix rotation_quaternion(const quaternion<T>& rotation);

		//	平行移動行列を生成
		static matrix translation(T position_x, T position_y);

		//	平行移動行列を生成
		static matrix translation(T position_x, T position_y, T position_z);

		//	平行移動行列を生成
		static matrix translation(T position_x, T position_y, T position_z, T position_w);

		//	平行移動行列を生成
		static matrix translation(const vector2<T>& position);

		//	平行移動行列を生成
		static matrix translation(const vector3<T>& position);

		//	平行移動行列を生成
		static matrix translation(const vector4<T>& position);

		//	姿勢制御行列を生成
		static matrix transform(const vector3<T>& scale, const quaternion<T>& rotation, const vector3<T>& position);

		//	姿勢制御行列を生成
		static matrix transform(const vector3<T>& scale, const vector3<T>& euler, const vector3<T>& position);

		//	視線行列を生成
		static matrix look_at(const vector3<T>& position, const vector3<T>& target, const vector3<T>& up_vector, coordinate_order coordinate = default_coordinate_order);

		//	透視投影行列生成
		//	@tool far が near 以下の場合は無限遠
		static matrix perspective(T width, T height, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	透視投影行列生成
		//	@tool far が near 未満の場合は無限遠
		static matrix perspective_fov(T fov_y, T aspect, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	矩形指定透視投影行列生成
		//	@tool far が near 未満の場合は無限遠
		static matrix perspective_off_center(T left, T right, T top, T bottom, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	透視投影行列生成
		//	@tool offset_x / offset_y は中心点の移動量
		static matrix perspective_fov_offset_center(T fov_y, T aspect, T near_z, T far_z, T offset_x, T offset_y, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	正射影行列生成
		static matrix orthographic(T width, T height, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	中心点移動付き正射影行列生成
		static matrix orthographic_offset_center(T left, T right, T top, T bottom, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	ビューポート行列生成
		static matrix viewport(T left, T right, T top, T bottom, T min_z, T max_z);

	public:
		union
		{
			register_type reg[4];
			T elements[element_count];
			T m[4][4];
			struct
			{
				T _11, _12, _13, _14;
				T _21, _22, _23, _24;
				T _31, _32, _33, _34;
				T _41, _42, _43, _44;
			};
		};
	};

	using matrixf = matrix<float>;
	using matrixd = matrix<double>;

	extern template class matrix<float>;
	extern template class matrix<double>;

}	//	namespace math
