#include "Math/math_all.h"

namespace math
{
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
	void vector3<T>::multiply(const quaternion<T>& q)
	{
		quaternion<T> work = q.create_conjugate() * quaternion<T>(x, y, z, 0) * q;
		set(work.x, work.y, work.z);
	}

	template<typename T>
	vector4<T> vector3<T>::create_transform(const matrix<T>& m) const
	{
		return transform(*this, m);
	}

	template<typename T>
	void vector3<T>::transform_coord(const matrix<T>& m)
	{
		*this = transform_coord(*this, m);
	}

	template<typename T>
	vector3<T> vector3<T>::create_transform_coord(const matrix<T>& m) const
	{
		return transform_coord(*this, m);
	}

	template<typename T>
	void vector3<T>::transform_normal(const matrix<T>& m)
	{
		*this = transform_normal(*this, m);
	}

	template<typename T>
	vector3<T> vector3<T>::create_transform_normal(const matrix<T>& m) const
	{
		return transform_normal(*this, m);
	}

	template<typename T>
	vector4<T> vector3<T>::transform(const vector3& l, const matrix<T>& r)
	{
		if constexpr (use_simd_v<T>)
		{
			return vector4<T>(simd::transform3_point<ops>(l.reg, r.reg));
		}
		else
		{
			return vector4<T>(l.x * r._11 + l.y * r._21 + l.z * r._31 + r._41,
							  l.x * r._12 + l.y * r._22 + l.z * r._32 + r._42,
							  l.x * r._13 + l.y * r._23 + l.z * r._33 + r._43,
							  l.x * r._14 + l.y * r._24 + l.z * r._34 + r._44);
		}
	}

	template<typename T>
	vector3<T> vector3<T>::transform_coord(const vector3& l, const matrix<T>& r)
	{
		if constexpr (use_simd_v<T>)
		{
			register_type v = simd::transform3_point<ops>(l.reg, r.reg);
			register_type w = ops::template splat<3>(v);
			//	@tool w が 0 のときは割らない（スカラー版と同じ挙動）
			register_type mask = ops::cmp_neq(w, ops::zero());
			return vector3(ops::select(mask, ops::div(v, w), v));
		}
		else
		{
			vector3 ret;
			ret.x = l.x * r._11 + l.y * r._21 + l.z * r._31 + r._41;
			ret.y = l.x * r._12 + l.y * r._22 + l.z * r._32 + r._42;
			ret.z = l.x * r._13 + l.y * r._23 + l.z * r._33 + r._43;
			T w = l.x * r._14 + l.y * r._24 + l.z * r._34 + r._44;
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
	vector3<T> vector3<T>::transform_normal(const vector3& l, const matrix<T>& r)
	{
		if constexpr (use_simd_v<T>)
		{
			return vector3(simd::transform3_normal<ops>(l.reg, r.reg));
		}
		else
		{
			//	@tool Framework のスカラー版は平行移動（_41～_43）も加算していたため、SIMD 版に合わせて除外している
			return vector3(l.x * r._11 + l.y * r._21 + l.z * r._31,
						   l.x * r._12 + l.y * r._22 + l.z * r._32,
						   l.x * r._13 + l.y * r._23 + l.z * r._33);
		}
	}

	template<typename T>
	vector3<T> vector3<T>::calc_screen_to_projection(T screen_x, T screen_y, T depth, const matrix<T>& projection, const matrix<T>& viewport)
	{
		//	各行列の逆行列を算出
		matrix<T> inverse_projection = projection.create_inverse();
		matrix<T> inverse_viewport = viewport.create_inverse();

		//	逆変換
		matrix<T> work = inverse_viewport * inverse_projection;
		return transform_coord(vector3(screen_x, screen_y, depth), work);
	}

	template<typename T>
	vector3<T> vector3<T>::calc_screen_to_world(T screen_x, T screen_y, T depth, const matrix<T>& view, const matrix<T>& projection, const matrix<T>& viewport)
	{
		//	各行列の逆行列を算出
		matrix<T> inverse_view = view.create_inverse();
		matrix<T> inverse_projection = projection.create_inverse();
		matrix<T> inverse_viewport = viewport.create_inverse();

		//	逆変換
		matrix<T> work = inverse_viewport * inverse_projection * inverse_view;
		return transform_coord(vector3(screen_x, screen_y, depth), work);
	}

	template class vector3<float>;
	template class vector3<double>;

}	//	namespace math
