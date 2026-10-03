#include "Math/math_all.h"

namespace math
{
	template<typename T>
	basic_rotator<T>::basic_rotator(const vector3<T>& euler)
		: x(euler.x), y(euler.y), z(euler.z)
	{
	}

	template<typename T>
	basic_rotator<T>::basic_rotator(const quaternion<T>& rotation, rotation_order order)
	{
		vector3<T> euler = rotation.euler_angle(order);
		x = euler.x;
		y = euler.y;
		z = euler.z;
	}

	template<typename T>
	basic_rotator<T>::basic_rotator(const matrix<T>& rotation, rotation_order order)
	{
		vector3<T> euler;
		matrix<T>::to_euler_angle(euler, rotation, order);
		x = euler.x;
		y = euler.y;
		z = euler.z;
	}

	template<typename T>
	vector3<T> basic_rotator<T>::front() const
	{
		return to_quaternion().front();
	}

	template<typename T>
	vector3<T> basic_rotator<T>::up() const
	{
		return to_quaternion().up();
	}

	template<typename T>
	vector3<T> basic_rotator<T>::right() const
	{
		return to_quaternion().right();
	}

	template<typename T>
	vector3<T> basic_rotator<T>::to_vector3() const
	{
		return vector3<T>(x, y, z);
	}

	template<typename T>
	quaternion<T> basic_rotator<T>::to_quaternion(rotation_order order) const
	{
		return quaternion<T>(to_vector3(), order);
	}

	template<typename T>
	vector3<T> basic_rotator<T>::rotate(const vector3<T>& v) const
	{
		return to_quaternion().rotate(v);
	}

	template<typename T>
	vector3<T> basic_rotator<T>::unrotate(const vector3<T>& v) const
	{
		return to_quaternion().conjugated().rotate(v);
	}

	template<typename T>
	basic_rotator<T> basic_rotator<T>::inverse() const
	{
		return basic_rotator(to_quaternion().conjugated());
	}

	template<typename T>
	void basic_rotator<T>::normalize()
	{
		x = angle::normalize(x);
		y = angle::normalize(y);
		z = angle::normalize(z);
	}

	template<typename T>
	void basic_rotator<T>::clamp(const basic_rotator& min_angle, const basic_rotator& max_angle)
	{
		for (std::uint32_t i = 0; i < element_count; ++i)
		{
			elements[i] = elements[i] < min_angle.elements[i] ? min_angle.elements[i] : (elements[i] > max_angle.elements[i] ? max_angle.elements[i] : elements[i]);
		}
	}

	template<typename T>
	bool basic_rotator<T>::nearly_equal(const basic_rotator& r, T tolerance) const
	{
		return std::abs(angle::difference(x, r.x)) <= tolerance
			&& std::abs(angle::difference(y, r.y)) <= tolerance
			&& std::abs(angle::difference(z, r.z)) <= tolerance;
	}

	template<typename T>
	basic_rotator<T> basic_rotator<T>::lerp(const basic_rotator& l, const basic_rotator& r, T value)
	{
		return l + (r - l).normalized() * value;
	}

	template<typename T>
	basic_rotator<T> basic_rotator<T>::from_direction(const vector3<T>& direction)
	{
		const vector3<T> d = default_coordinate_order == coordinate_order::lh ? direction.normalized() : -direction.normalized();
		const T sin_pitch = d.y < static_cast<T>(-1) ? static_cast<T>(-1) : (d.y > static_cast<T>(1) ? static_cast<T>(1) : d.y);
		return basic_rotator(-std::asin(sin_pitch), std::atan2(d.x, d.z), 0);
	}

	template<typename T>
	basic_rotator<T>::operator quaternion<T>() const
	{
		return to_quaternion();
	}

	template class basic_rotator<float>;
	template class basic_rotator<double>;

}	//	namespace math
