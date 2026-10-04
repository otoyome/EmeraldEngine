#include "Math/math_all.h"

namespace math
{
	template<typename T>
	vector4<T> vector2<T>::transformed(const matrix<T>& m) const
	{
		return transform(*this, m);
	}

	template<typename T>
	void vector2<T>::transform_coord(const matrix<T>& m)
	{
		*this = transform_coord(*this, m);
	}

	template<typename T>
	vector2<T> vector2<T>::transformed_coord(const matrix<T>& m) const
	{
		return transform_coord(*this, m);
	}

	namespace
	{
		//	行ベクトル規約の形の行列 r で (x, y, 0, 1) * r を計算する
		template<typename T>
		vector4<T> transform_row(const vector2<T>& l, const matrix<T>& r)
		{
			if constexpr (use_simd_v<T>)
			{
				using ops = simd_ops_t<T>;
				//	x * m[0] + y * m[1] + m[3]
				simd_register_t<T> result = r.reg[3];
				result = ops::add(result, ops::mul(ops::template splat<1>(l.reg), r.reg[1]));
				result = ops::add(result, ops::mul(ops::template splat<0>(l.reg), r.reg[0]));
				return vector4<T>(result);
			}
			else
			{
				return vector4<T>(l.x * r.s11 + l.y * r.s21 + r.s41,
								  l.x * r.s12 + l.y * r.s22 + r.s42,
								  l.x * r.s13 + l.y * r.s23 + r.s43,
								  l.x * r.s14 + l.y * r.s24 + r.s44);
			}
		}

	}	//	namespace

	template<typename T>
	vector4<T> vector2<T>::transform(const vector2& l, const matrix<T>& r)
	{
		return transform_row(l, r);
	}

	template<typename T>
	vector2<T> vector2<T>::transform_coord(const vector2& l, const matrix<T>& r)
	{
		vector4<T> v = transform(l, r);
		if constexpr (use_simd_v<T>)
		{
			register_type w = ops::template splat<3>(v.reg);
			register_type mask = ops::cmp_neq(w, ops::zero());
			return vector2(ops::select(mask, ops::div(v.reg, w), v.reg));
		}
		else
		{
			vector2 ret(v.x, v.y);
			if (v.w != static_cast<T>(0))
			{
				ret.x /= v.w;
				ret.y /= v.w;
			}
			return ret;
		}
	}

	template class vector2<float>;
	template class vector2<double>;

}	//	namespace math
