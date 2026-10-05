#include "Math/math_all.h"

namespace math
{
	template<typename T>
	rotator<T>::rotator(const vector3<T>& euler)
		: x(euler.x), y(euler.y), z(euler.z)
	{
	}

	template<typename T>
	rotator<T>::rotator(const quaternion<T>& rotation, rotation_order order)
	{
		vector3<T> euler = rotation.euler_angle(order);
		x = euler.x;
		y = euler.y;
		z = euler.z;
	}

	template<typename T>
	rotator<T>::rotator(const matrix<T>& rotation, rotation_order order)
	{
		vector3<T> euler;
		matrix<T>::to_euler_angle(euler, rotation, order);
		x = euler.x;
		y = euler.y;
		z = euler.z;
	}

	template<typename T>
	vector3<T> rotator<T>::front() const
	{
		return to_quaternion().front();
	}

	template<typename T>
	vector3<T> rotator<T>::up() const
	{
		return to_quaternion().up();
	}

	template<typename T>
	vector3<T> rotator<T>::right() const
	{
		return to_quaternion().right();
	}

	template<typename T>
	vector3<T> rotator<T>::to_vector3() const
	{
		return vector3<T>(x, y, z);
	}

	template<typename T>
	quaternion<T> rotator<T>::to_quaternion(rotation_order order) const
	{
		return quaternion<T>(to_vector3(), order);
	}

	template<typename T>
	vector3<T> rotator<T>::rotate(const vector3<T>& v) const
	{
		return to_quaternion().rotate(v);
	}

	template<typename T>
	vector3<T> rotator<T>::unrotate(const vector3<T>& v) const
	{
		return to_quaternion().conjugated().rotate(v);
	}

	template<typename T>
	rotator<T> rotator<T>::inverse() const
	{
		return rotator(to_quaternion().conjugated());
	}

	template<typename T>
	void rotator<T>::normalize()
	{
		x = angle::normalize(x);
		y = angle::normalize(y);
		z = angle::normalize(z);
	}

	template<typename T>
	void rotator<T>::clamp(const rotator& min_angle, const rotator& max_angle)
	{
		for (std::uint32_t i = 0; i < element_count; ++i)
		{
			elements[i] = elements[i] < min_angle.elements[i] ? min_angle.elements[i] : (elements[i] > max_angle.elements[i] ? max_angle.elements[i] : elements[i]);
		}
	}

	template<typename T>
	bool rotator<T>::nearly_equal(const rotator& r, T tolerance) const
	{
		return std::abs(angle::difference(x, r.x)) <= tolerance
			&& std::abs(angle::difference(y, r.y)) <= tolerance
			&& std::abs(angle::difference(z, r.z)) <= tolerance;
	}

	template<typename T>
	rotator<T> rotator<T>::lerp(const rotator& l, const rotator& r, T value)
	{
		return l + (r - l).normalized() * value;
	}

	template<typename T>
	rotator<T> rotator<T>::from_direction(const vector3<T>& direction)
	{
		const vector3<T> d = default_coordinate_order == coordinate_order::lh ? direction.normalized() : -direction.normalized();
		const T sin_pitch = d.y < static_cast<T>(-1) ? static_cast<T>(-1) : (d.y > static_cast<T>(1) ? static_cast<T>(1) : d.y);
		return rotator(-std::asin(sin_pitch), std::atan2(d.x, d.z), 0);
	}

	template<typename T>
	rotator<T>::operator quaternion<T>() const
	{
		return to_quaternion();
	}

	template class rotator<float>;
	template class rotator<double>;

}	//	namespace math
