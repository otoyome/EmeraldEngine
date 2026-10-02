#include "test_framework.h"

using namespace math;

namespace
{
	const rotation_order all_orders[] = { rotation_order::xyz, rotation_order::xzy, rotation_order::yxz, rotation_order::yzx, rotation_order::zxy, rotation_order::zyx };

	//	@tool 回転順序ごとの中間の軸（asin / atan2 で ±90 度に制限される軸）
	std::uint32_t middle_axis(rotation_order order)
	{
		switch (order)
		{
			case rotation_order::xyz: return 1;
			case rotation_order::xzy: return 2;
			case rotation_order::yxz: return 0;
			case rotation_order::yzx: return 2;
			case rotation_order::zxy: return 0;
			case rotation_order::zyx: return 1;
			default: return 0;
		}
	}
}

//	@tool 通常の角度での往復（行列・クォータニオン）
MATH_TEST_TYPED(euler_roundtrip)
{
	const double tol = std::is_same_v<T, float> ? 1e-3 : 1e-9;
	for (rotation_order order : all_orders)
	{
		const vector3<T> euler(static_cast<T>(0.4), static_cast<T>(-0.7), static_cast<T>(1.2));
		const matrix<T> rotation = matrix<T>::rotation_xyz(euler, order);
		vector3<T> out;
		CHECK(!matrix<T>::to_euler_angle(out, rotation, order));
		CHECK_NEAR(out, euler, tol);
		CHECK_NEAR(quaternion<T>(euler, order).euler_angle(order), euler, tol);
		CHECK_NEAR(matrix<T>(quaternion<T>::rotation_xyz(euler, order)), rotation, tol);

		quaternion<T> q;
		q.euler_angle(euler, order);
		T x, y, z;
		quaternion<T>::to_euler_angle(x, y, z, q, order);
		CHECK_NEAR(vector3<T>(x, y, z), euler, tol);
	}
}

//	@tool ジンバルロック（中間の軸が ±90 度）とその付近でも、復元した角度が同じ回転を表す
MATH_TEST_TYPED(euler_gimbal_lock)
{
	const double tol = 2e-3;
	const T half_pi = constants<T>::pi_half;
	const T middles[] = { half_pi, -half_pi, half_pi - static_cast<T>(1e-4), -half_pi + static_cast<T>(1e-4), half_pi - static_cast<T>(1e-6), static_cast<T>(0.3), static_cast<T>(-1.2), 0 };
	const T others[] = { 0, static_cast<T>(0.7), static_cast<T>(-2.5), static_cast<T>(3.0), static_cast<T>(-0.1) };
	for (rotation_order order : all_orders)
	{
		for (T middle : middles)
		{
			for (T a : others)
			{
				for (T b : others)
				{
					vector3<T> euler(a, b, a * static_cast<T>(0.5) - b);
					euler[middle_axis(order)] = middle;
					const matrix<T> rotation = matrix<T>::rotation_xyz(euler, order);

					vector3<T> from_matrix;
					matrix<T>::to_euler_angle(from_matrix, rotation, order);
					CHECK_NEAR(matrix<T>::rotation_xyz(from_matrix, order), rotation, tol);

					//	@tool float のクォータニオン経由（誤差が乗る経路）でも崩れない
					const vector3<T> from_quaternion = quaternion<T>::rotation_xyz(euler, order).euler_angle(order);
					CHECK_NEAR(matrix<T>::rotation_xyz(from_quaternion, order), rotation, tol);
				}
			}
		}
	}
}

//	@tool ジンバルロック時は戻り値が true になる
MATH_TEST_TYPED(euler_gimbal_flag)
{
	for (rotation_order order : all_orders)
	{
		vector3<T> euler(static_cast<T>(0.3), static_cast<T>(0.3), static_cast<T>(0.3));
		euler[middle_axis(order)] = constants<T>::pi_half;
		vector3<T> out;
		CHECK(matrix<T>::to_euler_angle(out, matrix<T>::rotation_xyz(euler, order), order));
	}
}

//	@tool オイラー角の正規化：範囲内に収まり、同じ回転を表す
MATH_TEST_TYPED(euler_normalize)
{
	const double tol = 2e-3;
	const T values[] = { static_cast<T>(2.0), static_cast<T>(-2.4), static_cast<T>(4.0), static_cast<T>(-7.0), static_cast<T>(0.5), static_cast<T>(1.8) };
	for (rotation_order order : all_orders)
	{
		const vector3<T> low = angle::euler::limit_min<T>(order);
		const vector3<T> high = angle::euler::limit_max<T>(order);
		CHECK(high[middle_axis(order)] == constants<T>::pi_half && low == -high);
		for (T a : values)
		{
			for (T b : values)
			{
				const vector3<T> input(a, b, -a);
				const vector3<T> normalized = angle::euler::normalize(input, order);
				for (std::uint32_t i = 0; i < 3; ++i)
				{
					CHECK(normalized[i] >= low[i] - static_cast<T>(1e-4) && normalized[i] <= high[i] + static_cast<T>(1e-4));
				}
				CHECK_NEAR(matrix<T>::rotation_xyz(normalized, order), matrix<T>::rotation_xyz(input, order), tol);
			}
		}
	}
}
