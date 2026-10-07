#pragma once

#include "Math/math_common.h"

namespace math
{
	//	4x4行列演算クラス
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
		matrix(const vector4<T>& axis_x, const vector4<T>& axis_y, const vector4<T>& axis_z, const vector4<T>& axis_w);
		matrix(const element_matrix<T, 4, 4>& source);
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit matrix(const element_matrix<U, 4, 4>& source)
		{
			for (std::uint32_t i = 0; i < element_count; ++i)
			{
				elements[i] = static_cast<T>(source.elements[i]);
			}
		}
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit matrix(const matrix<U>& mat)
		{
			for (std::uint32_t i = 0; i < element_count; ++i)
			{
				elements[i] = static_cast<T>(mat.elements[i]);
			}
		}
		matrix(const register_type& axis_x, const register_type& axis_y, const register_type& axis_z, const register_type& axis_w)
		{
			reg[0] = axis_x;
			reg[1] = axis_y;
			reg[2] = axis_z;
			reg[3] = axis_w;
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
			if constexpr (is_column_vector)
			{
				set_storage(m11, m21, m31, m41,
							m12, m22, m32, m42,
							m13, m23, m33, m43,
							m14, m24, m34, m44);
			}
			else
			{
				set_storage(m11, m12, m13, m14,
							m21, m22, m23, m24,
							m31, m32, m33, m34,
							m41, m42, m43, m44);
			}
		}

		void set_storage(T v11, T v12, T v13, T v14,
						 T v21, T v22, T v23, T v24,
						 T v31, T v32, T v33, T v34,
						 T v41, T v42, T v43, T v44)
		{
			if constexpr (use_simd_v<T>)
			{
				reg[0] = ops::set(v11, v12, v13, v14);
				reg[1] = ops::set(v21, v22, v23, v24);
				reg[2] = ops::set(v31, v32, v33, v34);
				reg[3] = ops::set(v41, v42, v43, v44);
			}
			else
			{
				storage[0][0] = v11; storage[0][1] = v12; storage[0][2] = v13; storage[0][3] = v14;
				storage[1][0] = v21; storage[1][1] = v22; storage[1][2] = v23; storage[1][3] = v24;
				storage[2][0] = v31; storage[2][1] = v32; storage[2][2] = v33; storage[2][3] = v34;
				storage[3][0] = v41; storage[3][1] = v42; storage[3][2] = v43; storage[3][3] = v44;
			}
		}

		void set(const vector4<T>& axis_x, const vector4<T>& axis_y, const vector4<T>& axis_z, const vector4<T>& axis_w);

		//	転置する
		void transpose() { *this = transpose(*this); }

		//	転置済み行列を返す
		matrix transposed() const { return transpose(*this); }

		//	逆行列化
		void invert() { *this = inverse(*this); }

		//	逆行列化済み行列を取得
		matrix inverted() const { return inverse(*this); }

		//	単位行列化
		void identity();

		//	S(Scale)R(Rotation)T(Translation)行列分解して各姿勢値を取得
		bool decompose(vector3<T>& out_scale, quaternion<T>& out_rotation, vector3<T>& out_position) const;

		//	行列式
		T determinant() const { return determinant(*this); }

		//	回転行列対象のミラーリング
		void mirror(mirroring_order order);

		//	回転行列対象のミラーリング
		matrix mirrored(mirroring_order order) const { matrix ret = *this; ret.mirror(order); return ret; }

		//	Vector4で要素を取得
		vector4<T> to_vector4(std::int32_t index) const;

		//	Vector3で要素を取得
		vector3<T> to_vector3(std::int32_t index) const;

		//	平行移動成分の取得 / 設定
		vector3<T> get_translation() const;
		void set_translation(const vector3<T>& position);

	public:
		//	足し算
		void addition(const matrix& r);

		//	引き算
		void subtract(const matrix& r);

		//	掛け算
		void multiply(T value);

		//	掛け算
		void multiply(const matrix& r);

		//	型変換
		float4x4 to_float4x4() const;
		double4x4 to_double4x4() const;
		float4x3 to_float4x3() const;

	public:
		T& operator()(std::uint32_t row, std::uint32_t column)
		{
			if constexpr (is_column_vector)
			{
				return storage[column][row];
			}
			else
			{
				return storage[row][column];
			}
		}
		const T& operator()(std::uint32_t row, std::uint32_t column) const
		{
			if constexpr (is_column_vector)
			{
				return storage[column][row];
			}
			else
			{
				return storage[row][column];
			}
		}
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
		//	乗算
		static matrix concatenate(const matrix& first, const matrix& second);

		//	行列式
		static T determinant(const matrix& mat);

		//	逆行列化
		static matrix inverse(const matrix& mat);
		static bool try_inverse(const matrix& mat, matrix& out);

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
		static matrix affine_transformation(const vector3<T>& scale, const quaternion<T>& rotation, const vector3<T>& position);

		//	姿勢制御行列を生成
		static matrix affine_transformation(const vector3<T>& scale, const vector3<T>& euler, const vector3<T>& position);

		//	拡縮行列を対象とした逆行列化
		static matrix inverse_for_scaling(const matrix& mat);

		//	回転行列を対象とした逆行列化
		static matrix inverse_for_rotation(const matrix& mat);

		//	平行移動行列を対象とした逆行列化
		static matrix inverse_for_translation(const matrix& mat);

		//	姿勢制御行列の逆行列を生成
		static matrix inverse_affine_transformation(const vector3<T>& scale, const quaternion<T>& rotation, const vector3<T>& position);

		//	拡縮しない姿勢制御行列の逆行列を生成
		static matrix inverse_rigid_transformation(const quaternion<T>& rotation, const vector3<T>& position);

		//	視線行列を生成
		static matrix look_at(const vector3<T>& position, const vector3<T>& target, const vector3<T>& up_vector, coordinate_order coordinate = default_coordinate_order);

		//	透視投影行列生成
		//	far が near 以下の場合は無限遠
		static matrix perspective(T width, T height, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	透視投影行列生成
		//	far が near 以下の場合は無限遠
		static matrix perspective_fov(T fov_y, T aspect, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	矩形指定透視投影行列生成
		//	far が near 以下の場合は無限遠
		static matrix perspective_off_center(T left, T right, T bottom, T top, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	透視投影行列生成
		//	offset_x / offset_y は中心点の移動量
		static matrix perspective_fov_offset_center(T fov_y, T aspect, T near_z, T far_z, T offset_x, T offset_y, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	正射影行列生成
		static matrix orthographic(T width, T height, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	中心点移動付き正射影行列生成
		static matrix orthographic_offset_center(T left, T right, T bottom, T top, T near_z, T far_z, coordinate_order coordinate = default_coordinate_order, projection_z_order z_order = default_projection_z_order);

		//	ビューポート行列生成
		static matrix viewport(T left, T right, T top, T bottom, T min_z, T max_z);

	public:
		union
		{
			register_type reg[4];
			T elements[element_count];
			T storage[4][4];
			struct
			{
				T s11, s12, s13, s14;
				T s21, s22, s23, s24;
				T s31, s32, s33, s34;
				T s41, s42, s43, s44;
			};
			struct
			{
#if MATH_COLUMN_VECTOR
				T _11, _21, _31, _41;
				T _12, _22, _32, _42;
				T _13, _23, _33, _43;
				T _14, _24, _34, _44;
#else
				T _11, _12, _13, _14;
				T _21, _22, _23, _24;
				T _31, _32, _33, _34;
				T _41, _42, _43, _44;
#endif
			};
		};
	};

	using matrixf = matrix<float>;
	using matrixd = matrix<double>;

	extern template class matrix<float>;
	extern template class matrix<double>;

}	//	namespace math
