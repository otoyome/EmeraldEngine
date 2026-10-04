#include "Math/math_all.h"

namespace math
{
	template<typename T>
	exponential_quaternion<T>::exponential_quaternion(const quaternion<T>& q)
	{
		if (std::abs(q.w) >= static_cast<T>(1) - constants<T>::epsilon ||
			q.x * q.x + q.y * q.y + q.z * q.z <= constants<T>::epsilon * static_cast<T>(10))
		{
			//	無回転クォータニオンとみなす
			x = 0;
			y = 0;
			z = 0;
		}
		else
		{
			constexpr T angle_limit = static_cast<T>(1) - constants<T>::epsilon;
			vector3<T> axis = q.axis();
			T clamped_w = q.w < -angle_limit ? -angle_limit : (q.w > angle_limit ? angle_limit : q.w);
			T theta = std::acos(clamped_w);
			x = axis.x * theta;
			y = axis.y * theta;
			z = axis.z * theta;
		}
	}

	template<typename T>
	quaternion<T> exponential_quaternion<T>::to_quaternion() const
	{
		vector3<T> axis(x, y, z);

		T length = axis.square_length();
		if (length <= constants<T>::epsilon)
		{
			return quaternion<T>(0, 0, 0, 1);
		}

		T angle = std::sqrt(length);
		T theta = std::sin(angle) / angle;
		return quaternion<T>(axis.x * theta, axis.y * theta, axis.z * theta, std::cos(angle)).normalized();
	}

	template<typename T>
	exponential_quaternion<T>::operator quaternion<T>() const
	{
		return to_quaternion();
	}

	template class exponential_quaternion<float>;
	template class exponential_quaternion<double>;

}	//	namespace math
