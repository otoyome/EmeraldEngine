#include "Math/math_all.h"

namespace math
{
	namespace
	{
		template<typename T>
		vector4<T> transform_row(const vector3<T>& l, const matrix<T>& r)
		{
			if constexpr (use_simd_v<T>)
			{
				return vector4<T>(simd::transform3_point<simd_ops_t<T>>(l.reg, r.reg));
			}
			else
			{
				return vector4<T>(l.x * r.s11 + l.y * r.s21 + l.z * r.s31 + r.s41,
								  l.x * r.s12 + l.y * r.s22 + l.z * r.s32 + r.s42,
								  l.x * r.s13 + l.y * r.s23 + l.z * r.s33 + r.s43,
								  l.x * r.s14 + l.y * r.s24 + l.z * r.s34 + r.s44);
			}
		}

		template<typename T>
		vector3<T> transform_coord_row(const vector3<T>& l, const matrix<T>& r)
		{
			if constexpr (use_simd_v<T>)
			{
				using ops = simd_ops_t<T>;
				using register_type = simd_register_t<T>;
				register_type v = simd::transform3_point<ops>(l.reg, r.reg);
				register_type w = ops::template splat<3>(v);
				register_type mask = ops::cmp_neq(w, ops::zero());
				return vector3<T>(ops::select(mask, ops::div(v, w), v));
			}
			else
			{
				vector3<T> ret;
				ret.x = l.x * r.s11 + l.y * r.s21 + l.z * r.s31 + r.s41;
				ret.y = l.x * r.s12 + l.y * r.s22 + l.z * r.s32 + r.s42;
				ret.z = l.x * r.s13 + l.y * r.s23 + l.z * r.s33 + r.s43;
				T w = l.x * r.s14 + l.y * r.s24 + l.z * r.s34 + r.s44;
				if (w != static_cast<T>(0))
				{
					ret.x /= w;
					ret.y /= w;
					ret.z /= w;
				}
				return ret;
			}
		}

		template<typename T>
		vector3<T> transform_normal_row(const vector3<T>& l, const matrix<T>& r)
		{
			if constexpr (use_simd_v<T>)
			{
				return vector3<T>(simd::transform3_normal<simd_ops_t<T>>(l.reg, r.reg));
			}
			else
			{
				return vector3<T>(l.x * r.s11 + l.y * r.s21 + l.z * r.s31,
								  l.x * r.s12 + l.y * r.s22 + l.z * r.s32,
								  l.x * r.s13 + l.y * r.s23 + l.z * r.s33);
			}
		}

	}	//	namespace

	template<typename T>
	vector3<T>::vector3(const vector2<T>& v, T set_z)
	{
		set(v.x, v.y, set_z);
	}

	template<typename T>
	vector3<T>::vector3(const vector4<T>& v)
	{
		if constexpr (use_simd_v<T>)
		{
			set(v.reg);
		}
		else
		{
			set(v.x, v.y, v.z);
		}
	}

	template<typename T>
	void vector3<T>::rotate(const quaternion<T>& q)
	{
		//	q * v * q^-1
		*this = q.rotate(*this);
	}

	template<typename T>
	vector3<T> vector3<T>::rotated(const quaternion<T>& q) const
	{
		return q.rotate(*this);
	}

	template<typename T>
	vector4<T> vector3<T>::transformed(const matrix<T>& m) const
	{
		return transform(*this, m);
	}

	template<typename T>
	void vector3<T>::transform_coord(const matrix<T>& m)
	{
		*this = transform_coord(*this, m);
	}

	template<typename T>
	vector3<T> vector3<T>::transformed_coord(const matrix<T>& m) const
	{
		return transform_coord(*this, m);
	}

	template<typename T>
	void vector3<T>::transform_normal(const matrix<T>& m)
	{
		*this = transform_normal(*this, m);
	}

	template<typename T>
	vector3<T> vector3<T>::transformed_normal(const matrix<T>& m) const
	{
		return transform_normal(*this, m);
	}

	template<typename T>
	vector4<T> vector3<T>::transform(const vector3& l, const matrix<T>& r)
	{
		return transform_row(l, r);
	}

	template<typename T>
	vector3<T> vector3<T>::transform_coord(const vector3& l, const matrix<T>& r)
	{
		return transform_coord_row(l, r);
	}

	template<typename T>
	vector3<T> vector3<T>::transform_normal(const vector3& l, const matrix<T>& r)
	{
		return transform_normal_row(l, r);
	}

	template<typename T>
	vector3<T> vector3<T>::calc_screen_to_view(T screen_x, T screen_y, T depth, const matrix<T>& projection, const matrix<T>& viewport)
	{
		//	äeçsóÒÇÃãtçsóÒÇéZèo
		matrix<T> inverse_projection = projection.inverted();
		matrix<T> inverse_viewport = viewport.inverted();

		//	ãtïœä∑
		matrix<T> work = matrix<T>::concatenate(inverse_viewport, inverse_projection);
		return transform_coord(vector3(screen_x, screen_y, depth), work);
	}

	template<typename T>
	vector3<T> vector3<T>::calc_screen_to_world(T screen_x, T screen_y, T depth, const matrix<T>& view, const matrix<T>& projection, const matrix<T>& viewport)
	{
		//	äeçsóÒÇÃãtçsóÒÇéZèo
		matrix<T> inverse_view = view.inverted();
		matrix<T> inverse_projection = projection.inverted();
		matrix<T> inverse_viewport = viewport.inverted();

		//	ãtïœä∑
		matrix<T> work = matrix<T>::concatenate(matrix<T>::concatenate(inverse_viewport, inverse_projection), inverse_view);
		return transform_coord(vector3(screen_x, screen_y, depth), work);
	}

	template class vector3<float>;
	template class vector3<double>;

}	//	namespace math
