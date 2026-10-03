#include "test_framework.h"

using namespace math;

//	@tool 生成・軸と角度
MATH_TEST_TYPED(quaternion_basic)
{
	const double tol = math_test::tolerance<T>();
	CHECK(quaternion<T>() == quaternion<T>(0, 0, 0, 1) && quaternion<T>::identity() == quaternion<T>());

	const vector3<T> axis = vector3<T>(1, 2, 2).normalized();
	const T angle = static_cast<T>(1.2);
	const quaternion<T> q = quaternion<T>::rotation_axis(axis, angle);
	CHECK_NEAR(q.length(), static_cast<T>(1), tol);
	CHECK_NEAR(q.axis(), axis, tol);
	CHECK_NEAR(q.angle(), angle, tol * 10);
	vector3<T> out_axis;
	T out_angle;
	q.axis_angle(out_axis, out_angle);
	CHECK_NEAR(out_axis, axis, tol);
	CHECK(quaternion<T>(axis, angle) == q);
	CHECK(quaternion<T>().axis() == vector3<T>());
	//	@tool 誤差で w が 1 をわずかに超えても NaN にならない
	CHECK(quaternion<T>(0, 0, 0, static_cast<T>(1) + constants<T>::epsilon * 4).angle() == 0);

	//	@tool 回転軸は正規化される前提だが、rotation_axis は結果を正規化する
	CHECK_NEAR(quaternion<T>::rotation_axis(axis * static_cast<T>(3), angle).length(), static_cast<T>(1), tol);

	CHECK_SAME_ROTATION(quaternion<T>::rotation_x(static_cast<T>(0.5)), quaternion<T>::rotation_axis(vector3<T>::x_axis(), static_cast<T>(0.5)), tol);
	CHECK_SAME_ROTATION(quaternion<T>::rotation_y(static_cast<T>(0.5)), quaternion<T>::rotation_axis(vector3<T>::y_axis(), static_cast<T>(0.5)), tol);
	CHECK_SAME_ROTATION(quaternion<T>::rotation_z(static_cast<T>(0.5)), quaternion<T>::rotation_axis(vector3<T>::z_axis(), static_cast<T>(0.5)), tol);
}

//	@tool ベクトルの回転と行列との一致
MATH_TEST_TYPED(quaternion_rotate)
{
	const double tol = math_test::tolerance<T>() * 5;
	const quaternion<T> q = quaternion<T>::rotation_axis(vector3<T>(1, -1, 2).normalized(), static_cast<T>(2.1));
	const vector3<T> v(3, -1, 0.5);
	CHECK_NEAR(q.rotate(v), vector3<T>::transform_normal(v, matrix<T>(q)), tol);
	CHECK_NEAR(q.rotate(v).length(), v.length(), tol);
	CHECK_NEAR(q.inverted().rotate(q.rotate(v)), v, tol);
	CHECK_NEAR(quaternion<T>::rotation_x(constants<T>::pi_half).rotate(vector3<T>(0, 1, 0)), vector3<T>(0, 0, 1), tol);
}

//	@tool 合成（concatenate / hamilton_product は規約に関係なく同じ、* は規約に従う）
MATH_TEST_TYPED(quaternion_concatenate)
{
	const double tol = math_test::tolerance<T>() * 5;
	const quaternion<T> a = quaternion<T>::rotation_axis(vector3<T>(1, 2, 3).normalized(), static_cast<T>(0.8));
	const quaternion<T> b = quaternion<T>::rotation_axis(vector3<T>(-2, 1, 0.5).normalized(), static_cast<T>(-1.3));
	const vector3<T> v(1, 2, 3);

	CHECK_NEAR(quaternion<T>::concatenate(a, b).rotate(v), b.rotate(a.rotate(v)), tol);
	CHECK_SAME_ROTATION(quaternion<T>::hamilton_product(b, a), quaternion<T>::concatenate(a, b), tol);
	CHECK_NEAR(matrix<T>(quaternion<T>::concatenate(a, b)), matrix<T>::concatenate(matrix<T>(a), matrix<T>(b)), tol);
	//	@tool どちらの規約でも matrix(a * b) == matrix(a) * matrix(b)
	CHECK_NEAR(matrix<T>(a * b), matrix<T>(a) * matrix<T>(b), tol);
#if MATH_COLUMN_VECTOR
	CHECK_SAME_ROTATION(a * b, quaternion<T>::concatenate(b, a), tol);
#else
	CHECK_SAME_ROTATION(a * b, quaternion<T>::concatenate(a, b), tol);
#endif
	quaternion<T> c = a;
	c *= b;
	CHECK_SAME_ROTATION(c, a * b, tol);
	c = a;
	c.multiply(b);
	CHECK_SAME_ROTATION(c, a * b, tol);
	CHECK_SAME_ROTATION(a * quaternion<T>(), a, tol);
}

//	@tool 共役・逆・正規化・内積・スカラー倍
MATH_TEST_TYPED(quaternion_algebra)
{
	const double tol = math_test::tolerance<T>() * 5;
	const quaternion<T> q = quaternion<T>::rotation_axis(vector3<T>(0, 1, 1).normalized(), static_cast<T>(0.7));
	CHECK(q.conjugated() == quaternion<T>(-q.x, -q.y, -q.z, q.w));
	CHECK_SAME_ROTATION(q * q.inverted(), quaternion<T>(), tol);
	CHECK_SAME_ROTATION(q.inverted() * q, quaternion<T>(), tol);

	const quaternion<T> scaled = q * static_cast<T>(3);
	CHECK_NEAR(scaled.length(), static_cast<T>(3), tol);
	CHECK_NEAR(scaled.normalized(), q, tol);
	CHECK_SAME_ROTATION(scaled.inverted(), q.inverted() * static_cast<T>(1.0 / 3.0), tol);
	CHECK_NEAR(q.dot(q), static_cast<T>(1), tol);
	CHECK_NEAR(q.square_length(), static_cast<T>(1), tol);

	//	@tool 長さが 0 のときは正規化しても変化しない
	const quaternion<T> zero(0, 0, 0, 0);
	CHECK(zero.normalized() == zero);
	CHECK(-q == quaternion<T>(-q.x, -q.y, -q.z, -q.w));
}

//	@tool 回転行列 -> クォータニオン（分岐の4経路すべて）
MATH_TEST_TYPED(quaternion_from_matrix)
{
	const double tol = math_test::tolerance<T>() * 5;
	const T pi = constants<T>::pi;
	const quaternion<T> cases[] =
	{
		quaternion<T>::rotation_axis(vector3<T>(1, 2, 3).normalized(), static_cast<T>(0.5)),
		quaternion<T>::rotation_axis(vector3<T>(1, 0, 0), pi - static_cast<T>(0.01)),
		quaternion<T>::rotation_axis(vector3<T>(0, 1, 0), pi - static_cast<T>(0.01)),
		quaternion<T>::rotation_axis(vector3<T>(0, 0, 1), pi - static_cast<T>(0.01)),
		quaternion<T>::rotation_axis(vector3<T>(1, 1, 0).normalized(), pi),
	};
	for (const quaternion<T>& q : cases)
	{
		CHECK_SAME_ROTATION(quaternion<T>::rotation_matrix(matrix<T>(q)), q, tol);
		CHECK_SAME_ROTATION(quaternion<T>(matrix<T>(q)), q, tol);
	}
}

//	@tool 2つの方向を合わせる回転（平行・逆向きを含む）
MATH_TEST_TYPED(quaternion_vector_to_vector)
{
	const double tol = math_test::tolerance<T>() * 5;
	const vector3<T> from = vector3<T>(1, 2, -1).normalized();
	const vector3<T> to = vector3<T>(-3, 1, 2).normalized();
	CHECK_NEAR(quaternion<T>::vector_to_vector(from, to).rotate(from), to, tol);
	CHECK_NEAR(quaternion<T>(from, to).rotate(from), to, tol);
	CHECK_SAME_ROTATION(quaternion<T>::vector_to_vector(from, from), quaternion<T>(), tol);
	CHECK_NEAR(quaternion<T>::vector_to_vector(from, -from).rotate(from), -from, tol);
	CHECK_NEAR(quaternion<T>::vector_to_vector(vector3<T>::x_axis(), -vector3<T>::x_axis()).rotate(vector3<T>::x_axis()), -vector3<T>::x_axis(), tol);
}

//	@tool 向き（前方向は座標系に従う）
MATH_TEST_TYPED(quaternion_direction)
{
	const double tol = math_test::tolerance<T>() * 5;
	CHECK_NEAR(quaternion<T>().front(), vector3<T>::forward(), tol);
	CHECK_NEAR(quaternion<T>().right(), vector3<T>::right(), tol);
	CHECK_NEAR(quaternion<T>().up(), vector3<T>::up(), tol);

	const quaternion<T> q = quaternion<T>::rotation_axis(vector3<T>(2, 1, -1).normalized(), static_cast<T>(1.1));
	CHECK_NEAR(q.front(coordinate_order::lh), q.rotate(vector3<T>(0, 0, 1)), tol);
	CHECK_NEAR(q.front(coordinate_order::rh), q.rotate(vector3<T>(0, 0, -1)), tol);
	CHECK_NEAR(q.right(), q.rotate(vector3<T>::right()), tol);
	CHECK_NEAR(q.up(), q.rotate(vector3<T>::up()), tol);

	const vector3<T> forward = vector3<T>(3, -1, 2).normalized();
	for (coordinate_order coordinate : { coordinate_order::lh, coordinate_order::rh })
	{
		const quaternion<T> look = quaternion<T>::look_rotation(forward, vector3<T>::up(), coordinate);
		CHECK_NEAR(look.front(coordinate), forward, tol);
		CHECK(vector3<T>::dot(look.up(), vector3<T>::up()) > static_cast<T>(0.5));
		CHECK_NEAR(vector3<T>::dot(look.right(), vector3<T>::up()), static_cast<T>(0), tol);
	}
	CHECK_NEAR(basic_rotator<T>(quaternion<T>::look_rotation(forward, vector3<T>::up())).front(), forward, tol);
}

//	@tool ミラーリング
MATH_TEST_TYPED(quaternion_mirror)
{
	const double tol = math_test::tolerance<T>() * 5;
	const quaternion<T> q = quaternion<T>::rotation_y(static_cast<T>(0.6));
	CHECK_NEAR(matrix<T>(q.mirrored(mirroring_order::x)), matrix<T>(q).mirrored(mirroring_order::x), tol);
	CHECK_NEAR(matrix<T>(q.mirrored(mirroring_order::y)), matrix<T>(q), tol);
	quaternion<T> m = q;
	m.mirror(mirroring_order::z);
	CHECK(m == q.mirrored(mirroring_order::z));
}

//	@tool 補間
MATH_TEST_TYPED(quaternion_interpolation)
{
	const double tol = math_test::tolerance<T>() * 5;
	const quaternion<T> a = quaternion<T>::rotation_y(static_cast<T>(0.2));
	const quaternion<T> b = quaternion<T>::rotation_y(static_cast<T>(1.4));
	CHECK_SAME_ROTATION(slerp(a, b, static_cast<T>(0)), a, tol);
	CHECK_SAME_ROTATION(slerp(a, b, static_cast<T>(1)), b, tol);
	CHECK_SAME_ROTATION(slerp(a, b, static_cast<T>(0.5)), quaternion<T>::rotation_y(static_cast<T>(0.8)), tol);
	//	@tool 符号が逆（同じ回転）の端点でも最短経路で補間する
	CHECK_SAME_ROTATION(slerp(a, -b, static_cast<T>(0.5)), quaternion<T>::rotation_y(static_cast<T>(0.8)), tol);
	CHECK_SAME_ROTATION(nlerp(a, b, static_cast<T>(0.5)), quaternion<T>::rotation_y(static_cast<T>(0.8)), tol);
	CHECK_SAME_ROTATION(nlerp(a, -b, static_cast<T>(1)), b, tol);
	CHECK_NEAR(lerp(a, b, static_cast<T>(0.5)), quaternion<T>((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2, (a.w + b.w) / 2), tol);
}

//	@tool 対数クォータニオン・rotator
MATH_TEST_TYPED(quaternion_exponential_rotator)
{
	const double tol = math_test::tolerance<T>() * 10;
	const quaternion<T> q = quaternion<T>::rotation_axis(vector3<T>(1, 2, -2).normalized(), static_cast<T>(1.2));
	const exponential_quaternion<T> log_q(q);
	CHECK_SAME_ROTATION(log_q.to_quaternion(), q, tol);
	//	@tool 対数の半分は半分の回転
	CHECK_SAME_ROTATION((log_q * static_cast<T>(0.5)).to_quaternion(), quaternion<T>::rotation_axis(vector3<T>(1, 2, -2).normalized(), static_cast<T>(0.6)), tol);
	CHECK(exponential_quaternion<T>(quaternion<T>()).to_quaternion() == quaternion<T>());

	const basic_rotator<T> rotator(static_cast<T>(0.3), static_cast<T>(-0.8), static_cast<T>(0.5));
	CHECK_SAME_ROTATION(rotator.to_quaternion(), quaternion<T>(rotator.to_vector3()), tol);
	CHECK_NEAR(basic_rotator<T>(rotator.to_quaternion()).to_vector3(), rotator.to_vector3(), tol);
	CHECK_SAME_ROTATION((rotator * rotator).to_quaternion(), rotator.to_quaternion() * rotator.to_quaternion(), tol);
}
