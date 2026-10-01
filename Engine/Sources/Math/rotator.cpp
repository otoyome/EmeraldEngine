#include "Math/math_all.h"

namespace math
{
	template<typename T>
	basic_rotator<T>::basic_rotator(const vector3<T>& euler)
		: x(euler.x), y(euler.y), z(euler.z)
	{
	}

	template<typename T>
	basic_rotator<T>::basic_rotator(const quaternion<T>& rotation)
	{
		vector3<T> euler = rotation.euler_angle();
		x = euler.x;
		y = euler.y;
		z = euler.z;
	}

	template<typename T>
	vector3<T> basic_rotator<T>::front() const
	{
		return to_quaternion().front().normalized();
	}

	template<typename T>
	vector3<T> basic_rotator<T>::up() const
	{
		return to_quaternion().up().normalized();
	}

	template<typename T>
	vector3<T> basic_rotator<T>::right() const
	{
		return to_quaternion().right().normalized();
	}

	template<typename T>
	vector3<T> basic_rotator<T>::to_vector3() const
	{
		return vector3<T>(x, y, z);
	}

	template<typename T>
	quaternion<T> basic_rotator<T>::to_quaternion() const
	{
		return quaternion<T>(to_vector3());
	}

	template<typename T>
	basic_rotator<T>::operator quaternion<T>() const
	{
		return to_quaternion();
	}

	template<typename T>
	basic_rotator<T> basic_rotator<T>::operator*(const basic_rotator& r) const
	{
		return basic_rotator(to_quaternion() * r.to_quaternion());
	}

	template<typename T>
	basic_rotator<T>& basic_rotator<T>::operator*=(const basic_rotator& r)
	{
		*this = (*this * r);
		return *this;
	}

	template<typename T>
	basic_rotator<T> basic_rotator<T>::operator*(const quaternion<T>& r) const
	{
		return basic_rotator(to_quaternion() * r);
	}

	template<typename T>
	basic_rotator<T>& basic_rotator<T>::operator*=(const quaternion<T>& r)
	{
		*this = (*this * r);
		return *this;
	}

	template class basic_rotator<float>;
	template class basic_rotator<double>;

}	//	namespace math
