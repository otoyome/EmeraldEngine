#include "Math/math_all.h"

namespace math
{
	template<typename T>
	vector4<T>::vector4(const vector2<T>& v1, const vector2<T>& v2)
	{
		set(v1.x, v1.y, v2.x, v2.y);
	}

	template<typename T>
	vector4<T>::vector4(const vector3<T>& v, T set_w)
	{
		set(v.x, v.y, v.z, set_w);
	}

	template<typename T>
	vector3<T> vector4<T>::to_vector3() const
	{
		return vector3<T>(*this);
	}

	template<typename T>
	vector4<T> vector4<T>::cross(const vector4& l, const vector4& center, const vector4& r)
	{
		if constexpr (use_simd_v<T>)
		{
			return vector4(simd::cross4<ops>(l.reg, center.reg, r.reg));
		}
		else
		{
			vector4 ret;
			ret.x =  l.y * (center.z * r.w - r.z * center.w) - l.z * (center.y * r.w - r.y * center.w) + l.w * (center.y * r.z - center.z * r.y);
			ret.y = -(l.x * (center.z * r.w - r.z * center.w) - l.z * (center.x * r.w - r.x * center.w) + l.w * (center.x * r.z - r.x * center.z));
			ret.z =  l.x * (center.y * r.w - r.y * center.w) - l.y * (center.x * r.w - r.x * center.w) + l.w * (center.x * r.y - r.x * center.y);
			ret.w = -(l.x * (center.y * r.z - r.y * center.z) - l.y * (center.x * r.z - r.x * center.z) + l.z * (center.x * r.y - r.x * center.y));
			return ret;
		}
	}

	template<typename T>
	vector4<T> vector4<T>::transform(const vector3<T>& l, const matrix<T>& r)
	{
		return vector3<T>::transform(l, r);
	}

	namespace
	{
		template<typename T>
		vector4<T> transform_row(const vector4<T>& l, const matrix<T>& r)
		{
			if constexpr (use_simd_v<T>)
			{
				return vector4<T>(simd::transform4<simd_ops_t<T>>(l.reg, r.reg));
			}
			else
			{
				return vector4<T>(l.x * r.s11 + l.y * r.s21 + l.z * r.s31 + l.w * r.s41,
								  l.x * r.s12 + l.y * r.s22 + l.z * r.s32 + l.w * r.s42,
								  l.x * r.s13 + l.y * r.s23 + l.z * r.s33 + l.w * r.s43,
								  l.x * r.s14 + l.y * r.s24 + l.z * r.s34 + l.w * r.s44);
			}
		}

	}	//	namespace

	template<typename T>
	vector4<T> vector4<T>::transform(const vector4& l, const matrix<T>& r)
	{
		return transform_row(l, r);
	}

	template class vector4<float>;
	template class vector4<double>;

}	//	namespace math
