#pragma once

#include "Math/math_common.h"
#include "Math/vector2.h"
#include "Math/vector3.h"
#include "Math/vector4.h"
#include "Math/quaternion.h"
#include "Math/utility.h"

namespace math
{
	namespace detail
	{
		template<typename V, typename T>
		V lerp_elements(const V& l, const V& r, T value)
		{
			return l + (r - l) * value;
		}

		template<typename T>
		quaternion<T> lerp_quaternion(const quaternion<T>& l, const quaternion<T>& r, T value)
		{
			if constexpr (use_simd_v<T>)
			{
				using ops = simd_ops_t<T>;
				return quaternion<T>(ops::add(l.reg, ops::mul(ops::sub(r.reg, l.reg), ops::set1(value))));
			}
			else
			{
				return quaternion<T>(l.x + (r.x - l.x) * value,
									 l.y + (r.y - l.y) * value,
									 l.z + (r.z - l.z) * value,
									 l.w + (r.w - l.w) * value);
			}
		}

		template<typename T>
		vector3<T> slerp_vector(const vector3<T>& l, const vector3<T>& r, T value)
		{
			T dot = vector3<T>::dot(l, r);
			dot = dot < static_cast<T>(-1) ? static_cast<T>(-1) : (dot > static_cast<T>(1) ? static_cast<T>(1) : dot);
			if (dot > static_cast<T>(1) - constants<T>::epsilon * static_cast<T>(16))
			{
				return (l + (r - l) * value).normalized();
			}

			if (dot < static_cast<T>(-1) + constants<T>::epsilon * static_cast<T>(16))
			{
				vector3<T> other = std::abs(l.x) < static_cast<T>(0.9) ? vector3<T>::x_axis() : vector3<T>::y_axis();
				vector3<T> axis = vector3<T>::cross(l, other).normalized();
				T rotate_angle = constants<T>::pi * value;
				return (l * std::cos(rotate_angle) + vector3<T>::cross(axis, l) * std::sin(rotate_angle)).normalized();
			}
			T angle = std::acos(dot);

			T sin_theta = std::sin(angle);
			T ps = std::sin(angle * (static_cast<T>(1) - value));
			T pe = std::sin(angle * value);
			return ((l * ps + r * pe) / sin_theta).normalized();
		}

		template<typename T>
		quaternion<T> slerp_quaternion(const quaternion<T>& l, const quaternion<T>& r, T value)
		{
			{
				T dot = l.dot(r);
				bool flag = false;
				if (dot < static_cast<T>(0))
				{
					flag = true;
					dot = -dot;
				}

				T opp;
				T inv;
				if (dot > static_cast<T>(0.999999))
				{
					inv = static_cast<T>(1) - value;
					opp = flag ? -value : value;
				}
				else
				{
					T ac = std::acos(dot);
					T inv_sin = static_cast<T>(1) / std::sin(ac);

					inv = std::sin((static_cast<T>(1) - value) * ac) * inv_sin;
					opp = flag ? (-std::sin(value * ac) * inv_sin)
							   : (+std::sin(value * ac) * inv_sin);
				}

				quaternion<T> ret;
				if constexpr (use_simd_v<T>)
				{
					using ops = simd_ops_t<T>;
					ret.reg = ops::add(ops::mul(l.reg, ops::set1(inv)), ops::mul(r.reg, ops::set1(opp)));
				}
				else
				{
					ret.set((inv * l.x) + (opp * r.x),
							(inv * l.y) + (opp * r.y),
							(inv * l.z) + (opp * r.z),
							(inv * l.w) + (opp * r.w));
				}
				ret.normalize();
				return ret;
			}
		}

		template<typename V, typename T>
		V smoothstep_elements(const V& l, const V& r, T value)
		{
			V ret;
			for (std::uint32_t i = 0; i < V::element_count; ++i)
			{
				T t = (value - l[i]) / (r[i] - l[i]);
				t = t < static_cast<T>(0) ? static_cast<T>(0) : (t > static_cast<T>(1) ? static_cast<T>(1) : t);
				ret[i] = t * t * (static_cast<T>(3) - (t + t));
			}
			return ret;
		}

	}	//	namespace detail

	//	線形補間
	inline vector2f lerp(const vector2f& l, const vector2f& r, float value) { return detail::lerp_elements(l, r, value); }
	inline vector2d lerp(const vector2d& l, const vector2d& r, double value) { return detail::lerp_elements(l, r, value); }
	inline vector2d lerp(const vector2d& l, const vector2d& r, float value) { return detail::lerp_elements(l, r, static_cast<double>(value)); }
	inline vector3f lerp(const vector3f& l, const vector3f& r, float value) { return detail::lerp_elements(l, r, value); }
	inline vector3d lerp(const vector3d& l, const vector3d& r, double value) { return detail::lerp_elements(l, r, value); }
	inline vector3d lerp(const vector3d& l, const vector3d& r, float value) { return detail::lerp_elements(l, r, static_cast<double>(value)); }
	inline vector4f lerp(const vector4f& l, const vector4f& r, float value) { return detail::lerp_elements(l, r, value); }
	inline vector4d lerp(const vector4d& l, const vector4d& r, double value) { return detail::lerp_elements(l, r, value); }
	inline vector4d lerp(const vector4d& l, const vector4d& r, float value) { return detail::lerp_elements(l, r, static_cast<double>(value)); }
	inline quaternionf lerp(const quaternionf& l, const quaternionf& r, float value) { return detail::lerp_quaternion(l, r, value); }
	inline quaterniond lerp(const quaterniond& l, const quaterniond& r, double value) { return detail::lerp_quaternion(l, r, value); }
	inline quaterniond lerp(const quaterniond& l, const quaterniond& r, float value) { return detail::lerp_quaternion(l, r, static_cast<double>(value)); }

	//	球面線形補間
	inline vector3f slerp(const vector3f& l, const vector3f& r, float value) { return detail::slerp_vector(l, r, value); }
	inline vector3d slerp(const vector3d& l, const vector3d& r, double value) { return detail::slerp_vector(l, r, value); }
	inline quaternionf slerp(const quaternionf& l, const quaternionf& r, float value) { return detail::slerp_quaternion(l, r, value); }
	inline quaterniond slerp(const quaterniond& l, const quaterniond& r, double value) { return detail::slerp_quaternion(l, r, value); }

	//	球面線形補間の近似
	template<typename T>
	quaternion<T> nlerp(const quaternion<T>& l, const quaternion<T>& r, detail::identity_t<T> value)
	{
		T sign = l.dot(r) < static_cast<T>(0) ? static_cast<T>(-1) : static_cast<T>(1);
		return detail::lerp_quaternion(l, r * sign, value).normalized();
	}

	//	エルミート補間
	inline vector2f smoothstep(const vector2f& l, const vector2f& r, float value) { return detail::smoothstep_elements(l, r, value); }
	inline vector2d smoothstep(const vector2d& l, const vector2d& r, double value) { return detail::smoothstep_elements(l, r, value); }
	inline vector3f smoothstep(const vector3f& l, const vector3f& r, float value) { return detail::smoothstep_elements(l, r, value); }
	inline vector3d smoothstep(const vector3d& l, const vector3d& r, double value) { return detail::smoothstep_elements(l, r, value); }
	inline vector4f smoothstep(const vector4f& l, const vector4f& r, float value) { return detail::smoothstep_elements(l, r, value); }
	inline vector4d smoothstep(const vector4d& l, const vector4d& r, double value) { return detail::smoothstep_elements(l, r, value); }

	//	3点用いたベジエ補間
	template<typename V, std::enable_if_t<detail::is_vector_v<V>, int> = 0>
	V bezier(const V& points0, const V& points1, const V& points2, detail::identity_t<typename V::value_type> value)
	{
		using T = typename V::value_type;
		T reverse_delta = static_cast<T>(1) - value;
		T delta1 = static_cast<T>(1) * reverse_delta * reverse_delta;
		T delta2 = static_cast<T>(2) * reverse_delta * value;
		T delta3 = static_cast<T>(1) * value * value;
		return points0 * delta1 + points1 * delta2 + points2 * delta3;
	}

	//	4点用いたベジエ補間
	template<typename V, std::enable_if_t<detail::is_vector_v<V>, int> = 0>
	V bezier(const V& points0, const V& points1, const V& points2, const V& points3, detail::identity_t<typename V::value_type> value)
	{
		using T = typename V::value_type;
		T reverse_delta = static_cast<T>(1) - value;
		T delta1 = static_cast<T>(1) * reverse_delta * reverse_delta * reverse_delta;
		T delta2 = static_cast<T>(3) * reverse_delta * reverse_delta * value;
		T delta3 = static_cast<T>(3) * reverse_delta * value * value;
		T delta4 = static_cast<T>(1) * value * value * value;
		return points0 * delta1 + points1 * delta2 + points2 * delta3 + points3 * delta4;
	}

}	//	namespace math
