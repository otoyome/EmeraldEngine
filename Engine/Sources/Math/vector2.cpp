#include "Math/math_all.h"

namespace math
{
	template<typename T>
	vector2<T>::vector2(const vector3<T>& v)
	{
		if constexpr (use_simd_v<T>)
		{
			set(v.reg);
		}
		else
		{
			set(v.x, v.y);
		}
	}

	template<typename T>
	vector2<T>::vector2(const vector4<T>& v)
	{
		if constexpr (use_simd_v<T>)
		{
			set(v.reg);
		}
		else
		{
			set(v.x, v.y);
		}
	}

	template<typename T>
	vector4<T> vector2<T>::create_transform(const matrix<T>& m) const
	{
		return transform(*this, m);
	}

	template<typename T>
	void vector2<T>::transform_coord(const matrix<T>& m)
	{
		*this = transform_coord(*this, m);
	}

	template<typename T>
	vector2<T> vector2<T>::create_transform_coord(const matrix<T>& m) const
	{
		return transform_coord(*this, m);
	}

	template<typename T>
	vector4<T> vector2<T>::transform(const vector2& l, const matrix<T>& r)
	{
		if constexpr (use_simd_v<T>)
		{
			//	@tool x * m[0] + y * m[1] + m[2] + m[3]
			register_type result = ops::add(r.reg[2], r.reg[3]);
			result = ops::add(result, ops::mul(ops::template splat<1>(l.reg), r.reg[1]));
			result = ops::add(result, ops::mul(ops::template splat<0>(l.reg), r.reg[0]));
			return vector4<T>(result);
		}
		else
		{
			return vector4<T>(l.x * r._11 + l.y * r._21 + r._31 + r._41,
							  l.x * r._12 + l.y * r._22 + r._32 + r._42,
							  l.x * r._13 + l.y * r._23 + r._33 + r._43,
							  l.x * r._14 + l.y * r._24 + r._34 + r._44);
		}
	}

	template<typename T>
	vector2<T> vector2<T>::transform_coord(const vector2& l, const matrix<T>& r)
	{
		if constexpr (use_simd_v<T>)
		{
			register_type v = transform(l, r).reg;
			register_type w = ops::template splat<3>(v);
			//	@tool w が 0 のときは割らない（スカラー版と同じ挙動）
			register_type mask = ops::cmp_neq(w, ops::zero());
			return vector2(ops::select(mask, ops::div(v, w), v));
		}
		else
		{
			vector2 ret;
			ret.x = l.x * r._11 + l.y * r._21 + r._31 + r._41;
			ret.y = l.x * r._12 + l.y * r._22 + r._32 + r._42;
			T w = l.x * r._14 + l.y * r._24 + r._34 + r._44;
			if (w != static_cast<T>(0))
			{
				ret.x /= w;
				ret.y /= w;
			}
			return ret;
		}
	}

	template class vector2<float>;
	template class vector2<double>;

}	//	namespace math
