#include <cassert>

#include "Math/math_all.h"

namespace math
{
	namespace
	{
		//	3要素を大きい順に並べた添字を返す
		template<typename T>
		void rank_decompose(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c, T x, T y, T z)
		{
			if (x < y)
			{
				if (y < z)
				{
					a = 2; b = 1; c = 0;
				}
				else
				{
					a = 1;
					if (x < z)
					{
						b = 2; c = 0;
					}
					else
					{
						b = 0; c = 2;
					}
				}
			}
			else
			{
				if (x < z)
				{
					a = 2; b = 0; c = 1;
				}
				else
				{
					a = 0;
					if (y < z)
					{
						b = 2; c = 1;
					}
					else
					{
						b = 1; c = 2;
					}
				}
			}
		}

	}	//	namespace

	template<typename T>
	matrix<T>::matrix(const vector4<T>& v1, const vector4<T>& v2, const vector4<T>& v3, const vector4<T>& v4)
	{
		set(v1, v2, v3, v4);
	}

	template<typename T>
	matrix<T>::matrix(const element_matrix<T, 4, 4>& source)
	{
		const auto& s = source.m;
		set_storage(s[0][0], s[0][1], s[0][2], s[0][3],
			s[1][0], s[1][1], s[1][2], s[1][3],
			s[2][0], s[2][1], s[2][2], s[2][3],
			s[3][0], s[3][1], s[3][2], s[3][3]);
	}

	template<typename T>
	matrix<T>::matrix(const quaternion<T>& rotation)
	{
		*this = rotation_quaternion(rotation);
	}

	template<typename T>
	matrix<T>::matrix(const quaternion<T>& rotation, const vector3<T>& position)
	{
		*this = rotation_quaternion(rotation);
		set_translation(position);
	}

	template<typename T>
	void matrix<T>::set(T value)
	{
		set(value, value, value, value,
			value, value, value, value,
			value, value, value, value,
			value, value, value, value);
	}

	template<typename T>
	void matrix<T>::set(const vector4<T>& v1, const vector4<T>& v2, const vector4<T>& v3, const vector4<T>& v4)
	{
		if constexpr (use_simd_v<T>)
		{
			reg[0] = v1.reg;
			reg[1] = v2.reg;
			reg[2] = v3.reg;
			reg[3] = v4.reg;
		}
		else
		{
			set_storage(v1.x, v1.y, v1.z, v1.w,
				v2.x, v2.y, v2.z, v2.w,
				v3.x, v3.y, v3.z, v3.w,
				v4.x, v4.y, v4.z, v4.w);
		}
	}

	template<typename T>
	void matrix<T>::identity()
	{
		set_storage(1, 0, 0, 0,
					0, 1, 0, 0,
					0, 0, 1, 0,
					0, 0, 0, 1);
	}

	template<typename T>
	bool matrix<T>::decompose(vector3<T>& out_scale, quaternion<T>& out_rotation, vector3<T>& out_position) const
	{
		const matrix& row_form = *this;
		out_position.set(row_form.s41, row_form.s42, row_form.s43);

		out_scale.set(std::sqrt(row_form.s11 * row_form.s11 + row_form.s12 * row_form.s12 + row_form.s13 * row_form.s13),
					  std::sqrt(row_form.s21 * row_form.s21 + row_form.s22 * row_form.s22 + row_form.s23 * row_form.s23),
					  std::sqrt(row_form.s31 * row_form.s31 + row_form.s32 * row_form.s32 + row_form.s33 * row_form.s33));

		std::uint32_t a, b, c;
		rank_decompose(a, b, c, out_scale.x, out_scale.y, out_scale.z);

		vector4<T> basis[3] = { row_form.to_vector4(0), row_form.to_vector4(1), row_form.to_vector4(2) };
		const vector4<T> identity_basis[3] =
		{
			vector4<T>(1, 0, 0, 0),
			vector4<T>(0, 1, 0, 0),
			vector4<T>(0, 0, 1, 0),
		};

		if (out_scale[a] < constants<T>::epsilon)
		{
			basis[a] = identity_basis[a];
		}
		basis[a].normalize();

		if (out_scale[b] < constants<T>::epsilon)
		{
			std::uint32_t aa, bb, cc;
			T abs_x = std::abs(basis[a].x);
			T abs_y = std::abs(basis[a].y);
			T abs_z = std::abs(basis[a].z);

			rank_decompose(aa, bb, cc, abs_x, abs_y, abs_z);

			basis[b] = vector4<T>(vector3<T>::cross(basis[a].to_vector3(), identity_basis[cc].to_vector3()));
		}
		basis[b].normalize();

		if (out_scale[c] < constants<T>::epsilon)
		{
			basis[c] = vector4<T>(vector3<T>::cross(basis[a].to_vector3(), basis[b].to_vector3()));
		}
		basis[c].normalize();

		matrix work;
		work.set_storage(basis[0].x, basis[0].y, basis[0].z, 0,
					basis[1].x, basis[1].y, basis[1].z, 0,
					basis[2].x, basis[2].y, basis[2].z, 0,
					0, 0, 0, 1);
		T det = work.determinant();
		if (det < static_cast<T>(0))	//	クラメルの公式で座標系の手のひらを確認する
		{
			// スケールを反転させて、X軸の基底ベクトルを反転させることにより座標系を切り替える。
			out_scale[a] = -out_scale[a];
			basis[a].negate();

			det = -det;
		}

		det -= static_cast<T>(1);
		det *= det;
		if (constants<T>::epsilon < det)
		{
			//	SRT行列ではないと判断
			return false;
		}

		work.set_storage(basis[0].x, basis[0].y, basis[0].z, 0,
				 basis[1].x, basis[1].y, basis[1].z, 0,
				 basis[2].x, basis[2].y, basis[2].z, 0,
				 0, 0, 0, 1);
		out_rotation = quaternion<T>::rotation_matrix(work);
		out_rotation.normalize();
		return true;
	}

	template<typename T>
	void matrix<T>::mirror(mirroring_order order)
	{
		//	http://momose-d.cocolog-nifty.com/blog/2014/08/post-ac70.html
		switch (order)
		{
			case mirroring_order::x:
			{
				s12 = -s12;
				s13 = -s13;
				s21 = -s21;
				s31 = -s31;
				break;
			}
			case mirroring_order::y:
			{
				s12 = -s12;
				s21 = -s21;
				s23 = -s23;
				s32 = -s32;
				break;
			}
			case mirroring_order::z:
			{
				s13 = -s13;
				s23 = -s23;
				s31 = -s31;
				s32 = -s32;
				break;
			}
		}
	}

	template<typename T>
	vector4<T> matrix<T>::to_vector4(std::int32_t index) const
	{
		if constexpr (use_simd_v<T>)
		{
			return vector4<T>(reg[index]);
		}
		else
		{
			return vector4<T>(storage[index][0], storage[index][1], storage[index][2], storage[index][3]);
		}
	}

	template<typename T>
	vector3<T> matrix<T>::to_vector3(std::int32_t index) const
	{
		if constexpr (use_simd_v<T>)
		{
			return vector3<T>(reg[index]);
		}
		else
		{
			return vector3<T>(storage[index][0], storage[index][1], storage[index][2]);
		}
	}

	template<typename T>
	vector3<T> matrix<T>::get_translation() const
	{
		return vector3<T>(s41, s42, s43);

	}

	template<typename T>
	void matrix<T>::set_translation(const vector3<T>& position)
	{
		s41 = position.x;
		s42 = position.y;
		s43 = position.z;
	}

	template<typename T>
	void matrix<T>::addition(const matrix& r)
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

	template<typename T>
	void matrix<T>::subtract(const matrix& r)
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

	template<typename T>
	void matrix<T>::multiply(T value)
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

	template<typename T>
	void matrix<T>::multiply(const matrix& r)
	{
		if constexpr (is_column_vector)
		{
			*this = concatenate(r, *this);
		}
		else
		{
			*this = concatenate(*this, r);
		}
	}

	template<typename T>
	float4x4 matrix<T>::to_float4x4() const
	{
		float4x4 ret;
		if constexpr (use_simd_v<T>)
		{
			for (std::int32_t row = 0; row < 4; ++row)
			{
				_mm_storeu_ps(ret.m[row], ops::to_float4(reg[row]));
			}
		}
		else
		{
			for (std::uint32_t i = 0; i < element_count; ++i)
			{
				ret.elements[i] = static_cast<float>(elements[i]);
			}
		}
		return ret;
	}

	template<typename T>
	double4x4 matrix<T>::to_double4x4() const
	{
		double4x4 ret;
		for (std::uint32_t i = 0; i < element_count; ++i)
		{
			ret.elements[i] = static_cast<double>(elements[i]);
		}
		return ret;
	}

	template<typename T>
	float4x3 matrix<T>::to_float4x3() const
	{
		float4x3 ret;
		for (std::int32_t row = 0; row < 4; ++row)
		{
			for (std::int32_t column = 0; column < 3; ++column)
			{
				ret.m[row][column] = static_cast<float>(storage[row][column]);
			}
		}
		return ret;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	matrix<T> matrix<T>::concatenate(const matrix& first, const matrix& second)
	{
		matrix ret;
		if constexpr (use_simd_v<T>)
		{
			simd::multiply<ops>(first.reg, second.reg, ret.reg);
		}
		else
		{
			for (std::int32_t axis = 0; axis < 4; ++axis)
			{
				for (std::int32_t component = 0; component < 4; ++component)
				{
					ret.storage[axis][component] = first.storage[axis][0] * second.storage[0][component]
													+ first.storage[axis][1] * second.storage[1][component]
													+ first.storage[axis][2] * second.storage[2][component]
													+ first.storage[axis][3] * second.storage[3][component];
				}
			}
		}
		return ret;
	}

	template<typename T>
	T matrix<T>::determinant(const matrix& mat)
	{
		if constexpr (use_simd_v<T>)
		{
			return simd::determinant<ops>(mat.reg);
		}
		else
		{
			vector4<T> v1(mat.storage[0][0], mat.storage[1][0], mat.storage[2][0], mat.storage[3][0]);
			vector4<T> v2(mat.storage[0][1], mat.storage[1][1], mat.storage[2][1], mat.storage[3][1]);
			vector4<T> v3(mat.storage[0][2], mat.storage[1][2], mat.storage[2][2], mat.storage[3][2]);
			vector4<T> minor = vector4<T>::cross(v1, v2, v3);
			return -(mat.storage[0][3] * minor.x + mat.storage[1][3] * minor.y + mat.storage[2][3] * minor.z + mat.storage[3][3] * minor.w);
		}
	}

	template<typename T>
	matrix<T> matrix<T>::inverse(const matrix& mat)
	{
		if constexpr (use_simd_v<T>)
		{
			matrix ret;
			if (!simd::inverse<ops>(mat.reg, ret.reg))
			{
				return mat;
			}
			return ret;
		}
		else
		{
			static const T sign[4] = { 1, -1, 1, -1 };

			T det = determinant(mat);
			if (det == static_cast<T>(0))
			{
				return mat;
			}

			matrix ret;
			for (std::int32_t i = 0; i < 4; ++i)
			{
				vector4<T> rows[3];
				for (std::int32_t j = 0; j < 4; ++j)
				{
					if (j != i)
					{
						std::int32_t a = j > i ? j - 1 : j;
						rows[a].set(mat.storage[j][0], mat.storage[j][1], mat.storage[j][2], mat.storage[j][3]);
					}
				}
				vector4<T> work = vector4<T>::cross(rows[0], rows[1], rows[2]);
				ret.storage[0][i] = sign[i] * work.x / det;
				ret.storage[1][i] = sign[i] * work.y / det;
				ret.storage[2][i] = sign[i] * work.z / det;
				ret.storage[3][i] = sign[i] * work.w / det;
			}
			return ret;
		}
	}

	template<typename T>
	bool matrix<T>::try_inverse(const matrix& mat, matrix& out)
	{
		if constexpr (use_simd_v<T>)
		{
			return simd::inverse<ops>(mat.reg, out.reg);
		}
		else
		{
			if (determinant(mat) == static_cast<T>(0))
			{
				return false;
			}
			out = inverse(mat);
			return true;
		}
	}

	template<typename T>
	matrix<T> matrix<T>::transpose(const matrix& mat)
	{
		matrix ret;
		if constexpr (use_simd_v<T>)
		{
			ops::transpose(mat.reg, ret.reg);
		}
		else
		{
			for (std::int32_t row = 0; row < 4; ++row)
			{
				for (std::int32_t column = 0; column < 4; ++column)
				{
					ret.storage[row][column] = mat.storage[column][row];
				}
			}
		}
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::scaling(const vector3<T>& scale)
	{
		return scaling(scale.x, scale.y, scale.z);
	}

	template<typename T>
	matrix<T> matrix<T>::scaling(T scale_x, T scale_y, T scale_z)
	{
		matrix ret;
		ret.s11 = scale_x;
		ret.s22 = scale_y;
		ret.s33 = scale_z;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_x(T pitch)
	{
		T sx, cx;
		math::angle::sin_cos(pitch, sx, cx);

		matrix ret;
		ret.s22 =  cx;
		ret.s23 =  sx;
		ret.s32 = -sx;
		ret.s33 =  cx;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_y(T yaw)
	{
		T sy, cy;
		math::angle::sin_cos(yaw, sy, cy);

		matrix ret;
		ret.s11 =  cy;
		ret.s13 = -sy;
		ret.s31 =  sy;
		ret.s33 =  cy;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_z(T roll)
	{
		T sz, cz;
		math::angle::sin_cos(roll, sz, cz);

		matrix ret;
		ret.s11 =  cz;
		ret.s12 =  sz;
		ret.s21 = -sz;
		ret.s22 =  cz;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_xyz(const vector3<T>& rotation, rotation_order order)
	{
		return rotation_xyz(rotation.x, rotation.y, rotation.z, order);
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_xyz(T pitch, T yaw, T roll, rotation_order order)
	{
		T sx, cx, sy, cy, sz, cz;
		math::angle::sin_cos(pitch, sx, cx);
		math::angle::sin_cos(yaw, sy, cy);
		math::angle::sin_cos(roll, sz, cz);

		matrix ret;
		switch (order)
		{
			case rotation_order::xyz:
			{
				ret.s11 = cy * cz;
				ret.s12 = cy * sz;
				ret.s13 = -sy;

				ret.s21 = sx * sy * cz + cx * -sz;
				ret.s22 = sx * sy * sz + cx * cz;
				ret.s23 = sx * cy;

				ret.s31 = cx * sy * cz + -sx * -sz;
				ret.s32 = cx * sy * sz + -sx * cz;
				ret.s33 = cx * cy;
				break;
			}
			case rotation_order::xzy:
			{
				ret.s11 = cz * cy;
				ret.s12 = sz;
				ret.s13 = cz * -sy;

				ret.s21 = cx * -sz * cy + sx * sy;
				ret.s22 = cx * cz;
				ret.s23 = cx * sz * sy + sx * cy;

				ret.s31 = sx * sz * cy + cx * sy;
				ret.s32 = -sx * cz;
				ret.s33 = -sx * sz * sy + cx * cy;
				break;
			}
			case rotation_order::yxz:
			{
				ret.s11 = cy * cz + -sy * -sx * -sz;
				ret.s12 = cy * sz + -sy * -sx * cz;
				ret.s13 = -sy * cx;

				ret.s21 = cx * -sz;
				ret.s22 = cx * cz;
				ret.s23 = sx;

				ret.s31 = sy * cz + cy * -sx * -sz;
				ret.s32 = sy * sz + cy * -sx * cz;
				ret.s33 = cy * cx;
				break;
			}
			case rotation_order::yzx:
			{
				ret.s11 = cy * cz;
				ret.s12 = cy * sz * cx + sy * sx;
				ret.s13 = cy * sz * sx + -sy * cx;

				ret.s21 = -sz;
				ret.s22 = cz * cx;
				ret.s23 = cz * sx;

				ret.s31 = sy * cz;
				ret.s32 = sy * sz * cx + cy * -sx;
				ret.s33 = sy * sz * sx + cy * cx;
				break;
			}
			case rotation_order::zxy:
			{
				ret.s11 = cz * cy + sz * sx * sy;
				ret.s12 = sz * cx;
				ret.s13 = cz * -sy + sz * sx * cy;

				ret.s21 = -sz * cy + cz * sx * sy;
				ret.s22 = cz * cx;
				ret.s23 = sz * sy + cz * sx * cy;

				ret.s31 = cx * sy;
				ret.s32 = -sx;
				ret.s33 = cx * cy;
				break;
			}
			case rotation_order::zyx:
			{
				ret.s11 = cz * cy;
				ret.s12 = sz * cx + cz * sy * sx;
				ret.s13 = sz * sx + cz * -sy * cx;

				ret.s21 = -sz * cy;
				ret.s22 = cz * cx + sz * sy * -sx;
				ret.s23 = cz * sx + sz * sy * cx;

				ret.s31 = sy;
				ret.s32 = cy * -sx;
				ret.s33 = cy * cx;
				break;
			}
			default:
			{
				break;
			}
		}
		return ret;
	}

	template<typename T>
	bool matrix<T>::to_euler_angle(T& out_x, T& out_y, T& out_z, const matrix& rotation, rotation_order order)
	{
		const T threshold = constants<T>::gimbal_lock_threshold;
		const auto& r = rotation.storage;
		switch (order)
		{
			case rotation_order::xyz:
			{
				T c = std::hypot(r[0][0], r[0][1]);
				out_y = std::atan2(-r[0][2], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	x == 0 と仮定
					out_x = 0;
					out_z = std::atan2(-r[1][0], r[1][1]);
					return true;
				}
				out_x = std::atan2(r[1][2], r[2][2]);
				out_z = std::atan2(r[0][1], r[0][0]);
				return false;
			}
			case rotation_order::xzy:
			{
				T c = std::hypot(r[0][0], r[0][2]);
				out_z = std::atan2(r[0][1], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	x == 0 と仮定
					out_x = 0;
					out_y = std::atan2(r[2][0], r[2][2]);
					return true;
				}
				out_x = std::atan2(-r[2][1], r[1][1]);
				out_y = std::atan2(-r[0][2], r[0][0]);
				return false;
			}
			case rotation_order::yxz:
			{
				T c = std::hypot(r[1][0], r[1][1]);
				out_x = std::atan2(r[1][2], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	y == 0 と仮定
					out_y = 0;
					out_z = std::atan2(r[0][1], r[0][0]);
					return true;
				}
				out_y = std::atan2(-r[0][2], r[2][2]);
				out_z = std::atan2(-r[1][0], r[1][1]);
				return false;
			}
			case rotation_order::yzx:
			{
				T c = std::hypot(r[1][1], r[1][2]);
				out_z = std::atan2(-r[1][0], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	y == 0 と仮定
					out_y = 0;
					out_x = std::atan2(-r[2][1], r[2][2]);
					return true;
				}
				out_x = std::atan2(r[1][2], r[1][1]);
				out_y = std::atan2(r[2][0], r[0][0]);
				return false;
			}
			case rotation_order::zxy:
			{
				T c = std::hypot(r[2][0], r[2][2]);
				out_x = std::atan2(-r[2][1], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	z == 0 と仮定
					out_z = 0;
					out_y = std::atan2(-r[0][2], r[0][0]);
					return true;
				}
				out_y = std::atan2(r[2][0], r[2][2]);
				out_z = std::atan2(r[0][1], r[1][1]);
				return false;
			}
			case rotation_order::zyx:
			{
				T c = std::hypot(r[2][1], r[2][2]);
				out_y = std::atan2(r[2][0], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	z == 0 と仮定
					out_z = 0;
					out_x = std::atan2(r[1][2], r[1][1]);
					return true;
				}
				out_x = std::atan2(-r[2][1], r[2][2]);
				out_z = std::atan2(-r[1][0], r[0][0]);
				return false;
			}
			default:
			{
				break;
			}
		}

		assert(!"NotComaptible RotationOrder.");
		return false;
	}

	template<typename T>
	bool matrix<T>::to_euler_angle(vector3<T>& out, const matrix& rotation, rotation_order order)
	{
		return to_euler_angle(out.x, out.y, out.z, rotation, order);
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_axis(const vector3<T>& axis, T angle)
	{
		return rotation_quaternion(quaternion<T>::rotation_axis(axis, angle));
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_quaternion(const quaternion<T>& rotation)
	{
		const T xx = rotation.x * rotation.x;
		const T xy = rotation.x * rotation.y;
		const T xz = rotation.x * rotation.z;
		const T yy = rotation.y * rotation.y;
		const T yz = rotation.y * rotation.z;
		const T zz = rotation.z * rotation.z;
		const T wx = rotation.w * rotation.x;
		const T wy = rotation.w * rotation.y;
		const T wz = rotation.w * rotation.z;
		constexpr T one = static_cast<T>(1);
		constexpr T two = static_cast<T>(2);

		matrix ret;
		ret.s11 = one - two * yy - two * zz;
		ret.s12 =       two * xy + two * wz;
		ret.s13 =       two * xz - two * wy;

		ret.s21 =       two * xy - two * wz;
		ret.s22 = one - two * xx - two * zz;
		ret.s23 =       two * yz + two * wx;

		ret.s31 =       two * xz + two * wy;
		ret.s32 =       two * yz - two * wx;
		ret.s33 = one - two * xx - two * yy;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::translation(T position_x, T position_y)
	{
		matrix ret;
		ret.s41 = position_x;
		ret.s42 = position_y;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::translation(T position_x, T position_y, T position_z)
	{
		matrix ret;
		ret.s41 = position_x;
		ret.s42 = position_y;
		ret.s43 = position_z;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::translation(T position_x, T position_y, T position_z, T position_w)
	{
		matrix ret;
		ret.s41 = position_x;
		ret.s42 = position_y;
		ret.s43 = position_z;
		ret.s44 = position_w;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::translation(const vector2<T>& position)
	{
		return translation(position.x, position.y);
	}

	template<typename T>
	matrix<T> matrix<T>::translation(const vector3<T>& position)
	{
		return translation(position.x, position.y, position.z);
	}

	template<typename T>
	matrix<T> matrix<T>::translation(const vector4<T>& position)
	{
		return translation(position.x, position.y, position.z, position.w);
	}

	template<typename T>
	matrix<T> matrix<T>::affine_transformation(const vector3<T>& scale, const quaternion<T>& rotation, const vector3<T>& position)
	{
		matrix ret = concatenate(scaling(scale), rotation_quaternion(rotation));
		ret.s41 = position.x;
		ret.s42 = position.y;
		ret.s43 = position.z;
		ret.s44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::affine_transformation(const vector3<T>& scale, const vector3<T>& euler, const vector3<T>& position)
	{
		matrix ret = concatenate(scaling(scale), rotation_xyz(euler));
		ret.s41 = position.x;
		ret.s42 = position.y;
		ret.s43 = position.z;
		ret.s44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::inverse_for_scaling(const matrix& mat)
	{
		matrix ret;
		ret.s11 = static_cast<T>(1) / mat.s11;
		ret.s22 = static_cast<T>(1) / mat.s22;
		ret.s33 = static_cast<T>(1) / mat.s33;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::inverse_for_rotation(const matrix& mat)
	{
		return transpose(mat);
	}

	template<typename T>
	matrix<T> matrix<T>::inverse_for_translation(const matrix& mat)
	{
		matrix ret;
		ret.set_translation(-mat.get_translation());
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::inverse_affine_transformation(const vector3<T>& scale, const quaternion<T>& rotation, const vector3<T>& position)
	{
		//	(S * R * T)^-1 = T^-1 * R^T * S^-1 を直接展開する
		const matrix r = rotation_quaternion(rotation);
		const T inverse_x = static_cast<T>(1) / scale.x;
		const T inverse_y = static_cast<T>(1) / scale.y;
		const T inverse_z = static_cast<T>(1) / scale.z;

		matrix ret;
		ret.s11 = r.s11 * inverse_x;	ret.s12 = r.s21 * inverse_y;	ret.s13 = r.s31 * inverse_z;	ret.s14 = static_cast<T>(0);
		ret.s21 = r.s12 * inverse_x;	ret.s22 = r.s22 * inverse_y;	ret.s23 = r.s32 * inverse_z;	ret.s24 = static_cast<T>(0);
		ret.s31 = r.s13 * inverse_x;	ret.s32 = r.s23 * inverse_y;	ret.s33 = r.s33 * inverse_z;	ret.s34 = static_cast<T>(0);
		ret.s41 = -(position.x * r.s11 + position.y * r.s12 + position.z * r.s13) * inverse_x;
		ret.s42 = -(position.x * r.s21 + position.y * r.s22 + position.z * r.s23) * inverse_y;
		ret.s43 = -(position.x * r.s31 + position.y * r.s32 + position.z * r.s33) * inverse_z;
		ret.s44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::inverse_rigid_transformation(const quaternion<T>& rotation, const vector3<T>& position)
	{
		//	(R * T)^-1 = T^-1 * R^T を直接展開する
		const matrix r = rotation_quaternion(rotation);

		matrix ret;
		ret.s11 = r.s11;	ret.s12 = r.s21;	ret.s13 = r.s31;	ret.s14 = static_cast<T>(0);
		ret.s21 = r.s12;	ret.s22 = r.s22;	ret.s23 = r.s32;	ret.s24 = static_cast<T>(0);
		ret.s31 = r.s13;	ret.s32 = r.s23;	ret.s33 = r.s33;	ret.s34 = static_cast<T>(0);
		ret.s41 = -(position.x * r.s11 + position.y * r.s12 + position.z * r.s13);
		ret.s42 = -(position.x * r.s21 + position.y * r.s22 + position.z * r.s23);
		ret.s43 = -(position.x * r.s31 + position.y * r.s32 + position.z * r.s33);
		ret.s44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::look_at(const vector3<T>& position, const vector3<T>& target, const vector3<T>& up_vector, coordinate_order coordinate)
	{
		vector3<T> axis_z;
		switch (coordinate)
		{
			case coordinate_order::rh:
			{
				axis_z = (position - target).normalized();
				break;
			}
			case coordinate_order::lh:
			default:
			{
				axis_z = (target - position).normalized();
				break;
			}
		}
		vector3<T> axis_x = vector3<T>::cross(up_vector, axis_z).normalized();
		vector3<T> axis_y = vector3<T>::cross(axis_z, axis_x).normalized();

		matrix ret;
		ret.set_storage(axis_x.x, axis_y.x, axis_z.x, 0,
					  axis_x.y, axis_y.y, axis_z.y, 0,
					  axis_x.z, axis_y.z, axis_z.z, 0,
					  -vector3<T>::dot(axis_x, position), -vector3<T>::dot(axis_y, position), -vector3<T>::dot(axis_z, position), 1);
		return ret;
	}

	namespace
	{
		//	透視投影の深度成分の設定
		template<typename T>
		void set_perspective_depth(matrix<T>& ret, T near_z, T far_z, bool infinite_far_plane, coordinate_order coordinate, projection_z_order z_order)
		{
			//	far に無限大を渡した場合も無限遠として扱う
			infinite_far_plane = infinite_far_plane || std::isinf(far_z);
			assert(!(far_z > static_cast<T>(0) && far_z < near_z));
			switch (coordinate)
			{
				case coordinate_order::rh:
				{
					if (infinite_far_plane)
					{
						if (z_order == projection_z_order::normal)
						{
							ret.s33 = static_cast<T>(-1);
							ret.s43 = -near_z;
						}
						else
						{
							ret.s33 = static_cast<T>(0);
							ret.s43 = near_z;
						}
					}
					else
					{
						if (z_order == projection_z_order::normal)
						{
							ret.s33 = far_z / (near_z - far_z);
							ret.s43 = near_z * ret.s33;
						}
						else
						{
							ret.s33 = near_z / (far_z - near_z);
							ret.s43 = far_z * ret.s33;
						}
					}
					ret.s34 = static_cast<T>(-1);
					break;
				}
				case coordinate_order::lh:
				default:
				{
					if (infinite_far_plane)
					{
						if (z_order == projection_z_order::normal)
						{
							ret.s33 = static_cast<T>(1);
							ret.s43 = -near_z;
						}
						else
						{
							ret.s33 = static_cast<T>(0);
							ret.s43 = near_z;
						}
					}
					else
					{
						if (z_order == projection_z_order::normal)
						{
							ret.s33 = far_z / (far_z - near_z);
							ret.s43 = -ret.s33 * near_z;
						}
						else
						{
							ret.s33 = -near_z / (far_z - near_z);
							ret.s43 = -far_z * ret.s33;
						}
					}
					ret.s34 = static_cast<T>(1);
					break;
				}
			}
			ret.s44 = static_cast<T>(0);
		}

	}	//	namespace

	template<typename T>
	matrix<T> matrix<T>::perspective(T width, T height, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		matrix ret;
		ret.s11 = static_cast<T>(2) * near_z / width;
		ret.s22 = static_cast<T>(2) * near_z / height;
		set_perspective_depth(ret, near_z, far_z, !(far_z > near_z), coordinate, z_order);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::perspective_fov(T fov_y, T aspect, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		T sin_fov, cos_fov;
		math::angle::sin_cos(static_cast<T>(0.5) * fov_y, sin_fov, cos_fov);
		T y_scale = cos_fov / sin_fov;
		T x_scale = y_scale / aspect;

		matrix ret;
		//	係数算出
		ret.s11 = x_scale;
		ret.s22 = y_scale;
		set_perspective_depth(ret, near_z, far_z, !(far_z > near_z), coordinate, z_order);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::perspective_off_center(T left, T right, T bottom, T top, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		//	XNDC	= a * XView / ZView + b;
		//	YNDC	= c * YView / ZView + d;
		//	係数
		//	a		= ( 2.0f * near ) / ( right -   left )
		//	c		= ( 2.0f * near ) / (   top - bottom )
		//	中心オフセット
		//	b		= ( right +   left ) / ( right -   left )
		//	d		= (   top + bottom ) / (   top - bottom )
		//	オフセットがない場合の各座標との関係( 逆算時に利用 )
		//	left	= +near * tan( 水平視野角 / 2 )
		//	right	= -near * tan( 水平視野角 / 2 )
		//	top		= +near * tan( 垂直視野角 / 2 )
		//	bottom	= -near * tan( 垂直視野角 / 2 )
		//	ZNDC	= e / ZView + f
		//	+0～+1
		//	e		= -far * near / ( far - near )
		//	f		= +far / ( far - near )
		//	+1～+0
		//	e		= far * near / ( far - near )
		//	f		= -near / ( far - near )
		//	-1～+1
		//	e		= -2.0f * ( far * near ) / ( far - near )
		//	f		= ( far + near ) / ( far - near )
		//	-+～-1	意味がない
		//

		T square_near = near_z + near_z;
		T reciprocal_width = static_cast<T>(1) / (right - left);
		T reciprocal_height = static_cast<T>(1) / (top - bottom);

		matrix ret;
		//	係数算出
		ret.s11 = square_near * reciprocal_width;
		ret.s22 = square_near * reciprocal_height;
		//	オフセット算出
		if (coordinate == coordinate_order::rh)
		{
			ret.s31 = (left + right) * reciprocal_width;
			ret.s32 = (top + bottom) * reciprocal_height;
		}
		else
		{
			ret.s31 = -(left + right) * reciprocal_width;
			ret.s32 = -(top + bottom) * reciprocal_height;
		}
		set_perspective_depth(ret, near_z, far_z, !(far_z > near_z), coordinate, z_order);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::perspective_fov_offset_center(T fov_y, T aspect, T near_z, T far_z, T offset_x, T offset_y, coordinate_order coordinate, projection_z_order z_order)
	{
		T height = std::tan(fov_y * static_cast<T>(0.5));
		T width = std::tan(math::angle::fov_vertical_to_fov_horizontal(fov_y, aspect) * static_cast<T>(0.5));
		return perspective_off_center(-near_z * width + offset_x,
									  +near_z * width + offset_x,
									  -near_z * height + offset_y,
									  +near_z * height + offset_y,
									  near_z,
									  far_z,
									  coordinate,
									  z_order);
	}

	template<typename T>
	matrix<T> matrix<T>::orthographic(T width, T height, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		matrix ret;
		ret.s11 = static_cast<T>(2) / width;
		ret.s22 = static_cast<T>(2) / height;

		//	far に無限大を渡した場合は無限遠の極限として扱う(深度は一定になる)
		if (std::isinf(far_z))
		{
			ret.s33 = static_cast<T>(0);
			ret.s43 = (z_order == projection_z_order::reverse) ? static_cast<T>(1) : static_cast<T>(0);
			ret.s44 = static_cast<T>(1);
			return ret;
		}

		//	Zオーダーが反転指定ならNear/Farを入れ替える
		if (z_order == projection_z_order::reverse)
		{
			T temp = near_z;
			near_z = far_z;
			far_z = temp;
		}

		switch (coordinate)
		{
			case coordinate_order::rh:
			{
				ret.s33 = static_cast<T>(1) / (near_z - far_z);
				ret.s43 = near_z / (near_z - far_z);
				break;
			}
			case coordinate_order::lh:
			default:
			{
				ret.s33 = static_cast<T>(1) / (far_z - near_z);
				ret.s43 = near_z / (near_z - far_z);
				break;
			}
		}

		ret.s44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::orthographic_offset_center(T left, T right, T bottom, T top, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		const bool infinite_far_plane = std::isinf(far_z);
		//	Zオーダーが反転指定ならNear/Farを入れ替える
		if (z_order == projection_z_order::reverse)
		{
			T temp = near_z;
			near_z = far_z;
			far_z = temp;
		}

		T reciprocal_width = static_cast<T>(1) / (right - left);
		T reciprocal_height = static_cast<T>(1) / (top - bottom);

		matrix ret;
		ret.s11 = reciprocal_width + reciprocal_width;
		ret.s22 = reciprocal_height + reciprocal_height;
		ret.s41 = -(left + right) * reciprocal_width;
		ret.s42 = -(top + bottom) * reciprocal_height;

		//	far に無限大を渡した場合は無限遠の極限として扱う(深度は一定になる)
		if (infinite_far_plane)
		{
			ret.s33 = static_cast<T>(0);
			ret.s43 = (z_order == projection_z_order::reverse) ? static_cast<T>(1) : static_cast<T>(0);
			ret.s44 = static_cast<T>(1);
			return ret;
		}
		switch (coordinate)
		{
			case coordinate_order::rh:
			{
				ret.s33 = static_cast<T>(1) / (near_z - far_z);
				ret.s43 = ret.s33 * near_z;
				break;
			}
			case coordinate_order::lh:
			default:
			{
				ret.s33 = static_cast<T>(1) / (far_z - near_z);
				ret.s43 = -ret.s33 * near_z;
				break;
			}
		}

		ret.s44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::viewport(T left, T right, T top, T bottom, T min_z, T max_z)
	{
		matrix ret;
		ret.s11 = (right - left) / static_cast<T>(2);
		ret.s12 = 0;
		ret.s13 = 0;
		ret.s14 = 0;

		ret.s21 = 0;
		ret.s22 = -(bottom - top) / static_cast<T>(2);
		ret.s23 = 0;
		ret.s24 = 0;

		ret.s31 = 0;
		ret.s32 = 0;
		ret.s33 = max_z - min_z;
		ret.s34 = 0;

		ret.s41 = left + ret.s11;
		ret.s42 = top - ret.s22;
		ret.s43 = min_z;
		ret.s44 = static_cast<T>(1);

		return ret;
	}

	template class matrix<float>;
	template class matrix<double>;

}	//	namespace math
