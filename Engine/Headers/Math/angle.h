#pragma once

#include "Math/math_common.h"
#include "Math/vector2.h"
#include "Math/vector3.h"
#include "Math/vector4.h"
#include "Math/matrix.h"

//	Šp“xŠÖŒW‚ÌŠÖ”ŒQ
namespace math::angle
{
	//	Degree > Radian ‚É•ÏŠ·
	template<typename T>
	constexpr T to_radian(T degree)
	{
		return degree * constants<T>::degree_to_radian;
	}

	//	Radian > Degree ‚É•ÏŠ·
	template<typename T>
	constexpr T to_degree(T radian)
	{
		return radian * constants<T>::radian_to_degree;
	}

	//	Degree > Radian ‚É•ÏŠ·
	template<typename T>
	vector2<T> to_radian(const vector2<T>& degree) { return degree * constants<T>::degree_to_radian; }
	template<typename T>
	vector3<T> to_radian(const vector3<T>& degree) { return degree * constants<T>::degree_to_radian; }
	template<typename T>
	vector4<T> to_radian(const vector4<T>& degree) { return degree * constants<T>::degree_to_radian; }

	//	Radian > Degree ‚É•ÏŠ·
	template<typename T>
	vector2<T> to_degree(const vector2<T>& radian) { return radian * constants<T>::radian_to_degree; }
	template<typename T>
	vector3<T> to_degree(const vector3<T>& radian) { return radian * constants<T>::radian_to_degree; }
	template<typename T>
	vector4<T> to_degree(const vector4<T>& radian) { return radian * constants<T>::radian_to_degree; }

	//	‚’¼‹–ìŠp‚©‚ç…•½‹–ìŠp‚É•ÏŠ·
	template<typename T>
	T fov_vertical_to_fov_horizontal(T fov_y, T aspect)
	{
		return static_cast<T>(2) * std::atan(std::tan(fov_y * static_cast<T>(0.5)) * aspect);
	}

	//	…•½‹–ìŠp‚©‚ç‚’¼‹–ìŠp‚É•ÏŠ·
	template<typename T>
	T fov_horizontal_to_fov_vertical(T fov_x, T aspect)
	{
		return static_cast<T>(2) * std::atan(std::tan(fov_x * static_cast<T>(0.5)) / aspect);
	}

	//	Šp“x‚Ì³‹K‰»
	template<typename T>
	T normalize(T angle)
	{
		return std::atan2(std::sin(angle), std::cos(angle));
	}

	//	Šp“x‚Ì·•ª‚ğZo
	template<typename T>
	T difference(T angle1, T angle2)
	{
		T diff = normalize(angle1) - normalize(angle2);
		if (diff > constants<T>::pi)
		{
			return diff - constants<T>::pi2;
		}
		else if (diff < -constants<T>::pi)
		{
			return diff + constants<T>::pi2;
		}
		return diff;
	}

	//	Úabc‚ª‰sŠp‚©”»’è
	template<typename T>
	bool is_sharp_angle(const vector3<T>& position_a, const vector3<T>& position_b, const vector3<T>& position_c)
	{
		return vector3<T>::dot(position_a - position_b, position_c - position_b) > static_cast<T>(0);
	}

	//	sincos‚Ì‹ß—’læ“¾—pŠÖ”
	template<typename T>
	void sin_cos(T angle, T& out_sin, T& out_cos)
	{
		if constexpr (std::is_same_v<T, float>)
		{
			//	https://homepage.math.uiowa.edu/~whan/3800.d/S4-6.pdf

			//	ƒ‰ƒWƒAƒ“Šp‚ğ -PI`PI‚ÌŠÔ‚É
			float quotient = angle / constants<float>::pi2;
			quotient = static_cast<float>(static_cast<std::int32_t>(angle >= 0.0f ? quotient + 0.5f : quotient - 0.5f));
			float y = angle - constants<float>::pi2 * quotient;

			//	y ‚Ìsin ‚ğ -PI/2`PI/2‚ÌŠÔ‚É
			float sign;
			if (y > constants<float>::pi_half)
			{
				y = constants<float>::pi - y;
				sign = -1.0f;
			}
			else if (y < -constants<float>::pi_half)
			{
				y = -constants<float>::pi - y;
				sign = -1.0f;
			}
			else
			{
				sign = +1.0f;
			}

			float square_y = y * y;

			//	11“x
			out_sin = (((((-2.3889859e-08f * square_y + 2.7525562e-06f) * square_y - 0.00019840874f) * square_y + 0.0083333310f) * square_y - 0.16666667f) * square_y + 1.0f) * y;

			//	10“x
			out_cos = sign * (((((-2.6051615e-07f * square_y + 2.4760495e-05f) * square_y - 0.0013888378f) * square_y + 0.041666638f) * square_y - 0.5f) * square_y + 1.0f);
		}
		else
		{
			out_sin = std::sin(angle);
			out_cos = std::cos(angle);
		}
	}

}	//	namespace math::angle

//	ƒIƒCƒ‰[Šp“xŠÖŒW‚ÌŠÖ”ŒQ
namespace math::angle::euler
{
	//	ƒIƒCƒ‰[Šp“x‚ÌÅ‘å
	template<typename T>
	vector3<T> limit_max(rotation_order order = default_rotation_order)
	{
		constexpr T pi = constants<T>::pi;
		constexpr T pi_half = constants<T>::pi_half;
		switch (order)
		{
			case rotation_order::xyz: return vector3<T>(pi, pi_half, pi);
			case rotation_order::xzy: return vector3<T>(pi, pi, pi_half);
			case rotation_order::yxz: return vector3<T>(pi_half, pi, pi);
			case rotation_order::yzx: return vector3<T>(pi, pi, pi_half);
			case rotation_order::zxy: return vector3<T>(pi_half, pi, pi);
			case rotation_order::zyx: return vector3<T>(pi, pi_half, pi);
		}
		return vector3<T>(pi, pi, pi_half);
	}

	//	ƒIƒCƒ‰[Šp“x‚ÌÅ¬
	template<typename T>
	vector3<T> limit_min(rotation_order order = default_rotation_order)
	{
		return -limit_max<T>(order);
	}

	//	ƒIƒCƒ‰[Šp“x‚Ì³‹K‰»
	template<typename T>
	vector3<T> normalize(const vector3<T>& angle, rotation_order order = default_rotation_order)
	{
		vector3<T> ret;
		matrix<T>::to_euler_angle(ret, matrix<T>::rotation_xyz(angle, order), order);
		return ret;
	}

}	//	namespace math::angle::euler
