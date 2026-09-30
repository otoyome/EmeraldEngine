#include <algorithm>

#include "Math/math_all.h"

namespace math
{
	template<typename T>
	quaternion<T>::quaternion(const vector3<T>& euler, rotation_order order)
	{
		*this = rotation_xyz(euler, order);
	}

	template<typename T>
	quaternion<T>::quaternion(const vector3<T>& rotation_axis_vector, T rotation_angle)
	{
		*this = rotation_axis(rotation_axis_vector, rotation_angle);
	}

	template<typename T>
	quaternion<T>::quaternion(const vector3<T>& vector1, const vector3<T>& vector2)
	{
		*this = vector_to_vector(vector1, vector2);
	}

	template<typename T>
	quaternion<T>::quaternion(const matrix<T>& m)
	{
		*this = rotation_matrix(m);
	}

	template<typename T>
	vector3<T> quaternion<T>::axis() const
	{
		T work = x * x + y * y + z * z;
		if (work <= constants<T>::epsilon)
		{
			return vector3<T>(0, 0, 0);
		}

		work = static_cast<T>(1) / std::sqrt(work);
		return vector3<T>(x * work, y * work, z * work);
	}

	template<typename T>
	void quaternion<T>::axis_angle(vector3<T>& out_axis, T& out_angle) const
	{
		out_axis = axis();
		out_angle = angle();
	}

	template<typename T>
	vector3<T> quaternion<T>::front() const
	{
		const T xx = x * x;
		const T xz = x * z;
		const T yy = y * y;
		const T yz = y * z;
		const T wx = w * x;
		const T wy = w * y;

		return vector3<T>(static_cast<T>(2) * xz + static_cast<T>(2) * wy,
						  static_cast<T>(2) * yz - static_cast<T>(2) * wx,
						  static_cast<T>(1) - static_cast<T>(2) * xx - static_cast<T>(2) * yy);
	}

	template<typename T>
	vector3<T> quaternion<T>::right() const
	{
		const T xy = x * y;
		const T xz = x * z;
		const T yy = y * y;
		const T zz = z * z;
		const T wy = w * y;
		const T wz = w * z;

		return vector3<T>(static_cast<T>(1) - static_cast<T>(2) * yy - static_cast<T>(2) * zz,
						  static_cast<T>(2) * xy + static_cast<T>(2) * wz,
						  static_cast<T>(2) * xz - static_cast<T>(2) * wy);
	}

	template<typename T>
	vector3<T> quaternion<T>::up() const
	{
		const T xx = x * x;
		const T xy = x * y;
		const T yz = y * z;
		const T zz = z * z;
		const T wx = w * x;
		const T wz = w * z;

		return vector3<T>(static_cast<T>(2) * xy - static_cast<T>(2) * wz,
						  static_cast<T>(1) - static_cast<T>(2) * xx - static_cast<T>(2) * zz,
						  static_cast<T>(2) * yz + static_cast<T>(2) * wx);
	}

	template<typename T>
	vector3<T> quaternion<T>::euler_angle(rotation_order order) const
	{
		vector3<T> ret;
		to_euler_angle(ret, *this, order);
		return ret;
	}

	template<typename T>
	void quaternion<T>::euler_angle(const vector3<T>& euler, rotation_order order)
	{
		*this = rotation_xyz(euler, order);
	}

	template<typename T>
	void quaternion<T>::mirroring(mirroring_order order)
	{
		//	http://momose-d.cocolog-nifty.com/blog/2014/08/post-ee2b.html
		switch (order)
		{
			case mirroring_order::x:
			{
				y = -y;
				z = -z;
				break;
			}
			case mirroring_order::y:
			{
				x = -x;
				z = -z;
				break;
			}
			case mirroring_order::z:
			{
				x = -x;
				y = -y;
				break;
			}
		}
	}

	template<typename T>
	bool quaternion<T>::restrict(const vector3<T>& min_angle, const vector3<T>& max_angle)
	{
		bool limit = false;
		for (std::uint32_t i = 0; i < vector3<T>::element_count; ++i)
		{
			T min_value = std::sin(min_angle[i] * static_cast<T>(0.5));
			T max_value = std::sin(max_angle[i] * static_cast<T>(0.5));
			if (elements[i] < min_value)
			{
				elements[i] = min_value;
				limit = true;
			}
			if (elements[i] > max_value)
			{
				elements[i] = max_value;
				limit = true;
			}
		}
		return limit;
	}

	///////////////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	quaternion<T> quaternion<T>::rotation_x(T pitch)
	{
		T s, c;
		math::angle::sin_cos(pitch * static_cast<T>(0.5), s, c);
		return quaternion(s, 0, 0, c);
	}

	template<typename T>
	quaternion<T> quaternion<T>::rotation_y(T yaw)
	{
		T s, c;
		math::angle::sin_cos(yaw * static_cast<T>(0.5), s, c);
		return quaternion(0, s, 0, c);
	}

	template<typename T>
	quaternion<T> quaternion<T>::rotation_z(T roll)
	{
		T s, c;
		math::angle::sin_cos(roll * static_cast<T>(0.5), s, c);
		return quaternion(0, 0, s, c);
	}

	template<typename T>
	quaternion<T> quaternion<T>::rotation_xyz(const vector3<T>& rotation, rotation_order order)
	{
		return rotation_xyz(rotation.x, rotation.y, rotation.z, order);
	}

	template<typename T>
	quaternion<T> quaternion<T>::rotation_xyz(T pitch, T yaw, T roll, rotation_order order)
	{
		T sx, cx, sy, cy, sz, cz;
		math::angle::sin_cos(pitch * static_cast<T>(0.5), sx, cx);
		math::angle::sin_cos(yaw * static_cast<T>(0.5), sy, cy);
		math::angle::sin_cos(roll * static_cast<T>(0.5), sz, cz);
		quaternion rotation;
		switch (order)
		{
			case rotation_order::xyz:
			{
				rotation = quaternion(cz *  cy * sx - sz * sy * cx,
									  cz *  sy * cx + sz * cy * sx,
									  cz * -sy * sx + sz * cy * cx,
									  cz *  cy * cx + sz * sy * sx);
				break;
			}
			case rotation_order::xzy:
			{
				rotation = quaternion(cy * cz * sx + sy * sz * cx,
									  cy * sz * sx + sy * cz * cx,
									  cy * sz * cx - sy * cz * sx,
									  cy * cz * cx - sy * sz * sx);
				break;
			}
			case rotation_order::yxz:
			{
				rotation = quaternion(cz * sx * cy - sz * cx * sy,
									  cz * cx * sy + sz * sx * cy,
									  cz * sx * sy + sz * cx * cy,
									  cz * cx * cy - sz * sx * sy);
				break;
			}
			case rotation_order::yzx:
			{
				rotation = quaternion(cx * -sz * sy + sx * cz * cy,
									  cx *  cz * sy - sx * sz * cy,
									  cx *  sz * cy + sx * cz * sy,
									  cx *  cz * cy + sx * sz * sy);
				break;
			}
			case rotation_order::zxy:
			{
				rotation = quaternion(cy *  sx * cz + sy * cx * sz,
									  cy * -sx * sz + sy * cx * cz,
									  cy *  cx * sz - sy * sx * cz,
									  cy *  cx * cz + sy * sx * sz);
				break;
			}
			case rotation_order::zyx:
			{
				rotation = quaternion(cx * sy * sz + sx * cy * cz,
									  cx * sy * cz - sx * cy * sz,
									  cx * cy * sz + sx * sy * cz,
									  cx * cy * cz - sx * sy * sz);
				break;
			}
			default:
			{
				break;
			}
		}
		return rotation;
	}

	template<typename T>
	bool quaternion<T>::to_euler_angle(vector3<T>& out, const quaternion& rotation, rotation_order order)
	{
		return matrix<T>::to_euler_angle(out.x, out.y, out.z, matrix<T>(rotation), order);
	}

	template<typename T>
	bool quaternion<T>::to_euler_angle(T& out_x, T& out_y, T& out_z, const quaternion& rotation, rotation_order order)
	{
		return matrix<T>::to_euler_angle(out_x, out_y, out_z, matrix<T>(rotation), order);
	}

	template<typename T>
	quaternion<T> quaternion<T>::rotation_axis(const vector3<T>& rotation_axis_vector, T rotation_angle)
	{
		T s, c;
		math::angle::sin_cos(rotation_angle * static_cast<T>(0.5), s, c);

		quaternion ret(rotation_axis_vector.x * s, rotation_axis_vector.y * s, rotation_axis_vector.z * s, c);
		ret.normalize();
		return ret;
	}

	template<typename T>
	quaternion<T> quaternion<T>::vector_to_vector(const vector3<T>& direction1, const vector3<T>& direction2)
	{
		vector3<T> rotation_axis_vector = vector3<T>::cross(direction1, direction2);
		T cos_angle = vector3<T>::dot(direction1, direction2);
		if (rotation_axis_vector.square_length() < constants<T>::normalize_threshold)
		{
			if (cos_angle >= static_cast<T>(0))
			{
				return quaternion(0, 0, 0, 1);
			}

			//	@tool Framework ÇÕãtå¸Ç´ÇÃÇ∆Ç´Ç‡ (0, 0, 0, -1)Åi= ñ≥âÒì]ÅjÇï‘ÇµÇƒÇ¢ÇΩÇΩÇﬂÅA
			//	@tool direction1 Ç…íºåÇ∑ÇÈé≤Ç≈ 180 ìxâÒì]Ç≥ÇπÇÈÇÊÇ§èCê≥ÇµÇƒÇ¢ÇÈ
			vector3<T> other = std::abs(direction1.x) < static_cast<T>(0.9) ? vector3<T>::x_axis() : vector3<T>::y_axis();
			vector3<T> half_axis = vector3<T>::cross(direction1, other).create_normalize();
			return quaternion(half_axis.x, half_axis.y, half_axis.z, 0);
		}
		rotation_axis_vector.normalize();

		//	îºäpÇÃåˆéÆ
		//	@tool ä€ÇﬂåÎç∑Ç≈ sqrt ÇÃíÜÇ™ïâÇ…Ç»ÇÁÇ»Ç¢ÇÊÇ§ 0 ñ¢ñûÇêÿÇËéÃÇƒÇƒÇ¢ÇÈ
		T c = std::sqrt((std::max)(static_cast<T>(0), static_cast<T>(0.5) * (static_cast<T>(1) + cos_angle)));
		T s = std::sqrt((std::max)(static_cast<T>(0), static_cast<T>(0.5) * (static_cast<T>(1) - cos_angle)));
		return quaternion(rotation_axis_vector.x * s, rotation_axis_vector.y * s, rotation_axis_vector.z * s, c);
	}

	template<typename T>
	quaternion<T> quaternion<T>::rotation_matrix(const matrix<T>& m)
	{
		quaternion ret;
		if (m._33 <= static_cast<T>(0))	//	x^2 + y^2 >= z^2 + w^2
		{
			T dif10 = m._22 - m._11;
			T omr22 = static_cast<T>(1) - m._33;
			if (dif10 <= static_cast<T>(0))	//	x^2 >= y^2
			{
				T four_x_sqr = omr22 - dif10;
				T inv4x = static_cast<T>(0.5) / std::sqrt(four_x_sqr);
				ret.set(four_x_sqr * inv4x,
						(m._12 + m._21) * inv4x,
						(m._13 + m._31) * inv4x,
						(m._23 - m._32) * inv4x);
			}
			else	//	y^2 >= x^2
			{
				T four_y_sqr = omr22 + dif10;
				T inv4y = static_cast<T>(0.5) / std::sqrt(four_y_sqr);
				ret.set((m._12 + m._21) * inv4y,
						four_y_sqr * inv4y,
						(m._23 + m._32) * inv4y,
						(m._31 - m._13) * inv4y);
			}
		}
		else	//	z^2 + w^2 >= x^2 + y^2
		{
			T sum10 = m._22 + m._11;
			T opr22 = static_cast<T>(1) + m._33;
			if (sum10 <= static_cast<T>(0))	//	z^2 >= w^2
			{
				T four_z_sqr = opr22 - sum10;
				T inv4z = static_cast<T>(0.5) / std::sqrt(four_z_sqr);
				ret.set((m._13 + m._31) * inv4z,
						(m._23 + m._32) * inv4z,
						four_z_sqr * inv4z,
						(m._12 - m._21) * inv4z);
			}
			else	//	w^2 >= z^2
			{
				T four_w_sqr = opr22 + sum10;
				T inv4w = static_cast<T>(0.5) / std::sqrt(four_w_sqr);
				ret.set((m._23 - m._32) * inv4w,
						(m._31 - m._13) * inv4w,
						(m._12 - m._21) * inv4w,
						four_w_sqr * inv4w);
			}
		}
		return ret;
	}

	template<typename T>
	quaternion<T> quaternion<T>::look_rotation(const vector3<T>& forward, const vector3<T>& up_vector)
	{
		vector3<T> axis_z = forward.create_normalize();
		vector3<T> axis_x = vector3<T>::cross(up_vector, axis_z).create_normalize();
		vector3<T> axis_y = vector3<T>::cross(axis_z, axis_x).create_normalize();

		T num8 = (axis_x.x + axis_y.y) + axis_z.z;
		if (num8 > static_cast<T>(0))
		{
			T num = std::sqrt(num8 + static_cast<T>(1));
			T ret_w = num * static_cast<T>(0.5);
			num = static_cast<T>(0.5) / num;
			return quaternion((axis_y.z - axis_z.y) * num,
							  (axis_z.x - axis_x.z) * num,
							  (axis_x.y - axis_y.x) * num,
							  ret_w);
		}
		if ((axis_x.x >= axis_y.y) && (axis_x.x >= axis_z.z))
		{
			T num7 = std::sqrt(((static_cast<T>(1) + axis_x.x) - axis_y.y) - axis_z.z);
			T num4 = static_cast<T>(0.5) / num7;
			return quaternion(static_cast<T>(0.5) * num7,
							  (axis_x.y + axis_y.x) * num4,
							  (axis_x.z + axis_z.x) * num4,
							  (axis_y.z - axis_z.y) * num4);
		}
		if (axis_y.y > axis_z.z)
		{
			T num6 = std::sqrt(((static_cast<T>(1) + axis_y.y) - axis_x.x) - axis_z.z);
			T num3 = static_cast<T>(0.5) / num6;
			return quaternion((axis_y.x + axis_x.y) * num3,
							  static_cast<T>(0.5) * num6,
							  (axis_z.y + axis_y.z) * num3,
							  (axis_z.x - axis_x.z) * num3);
		}

		T num5 = std::sqrt(((static_cast<T>(1) + axis_z.z) - axis_x.x) - axis_y.y);
		T num2 = static_cast<T>(0.5) / num5;
		return quaternion((axis_z.x + axis_x.z) * num2,
						  (axis_z.y + axis_y.z) * num2,
						  static_cast<T>(0.5) * num5,
						  (axis_x.y - axis_y.x) * num2);
	}

	template class quaternion<float>;
	template class quaternion<double>;

}	//	namespace math
