#include "test_framework.h"

using namespace math;

//	@tool 生成・設定・要素アクセス
MATH_TEST_TYPED(vector_construct)
{
	CHECK(vector2<T>() == vector2<T>(0, 0));
	CHECK(vector3<T>() == vector3<T>(0, 0, 0));
	CHECK(vector4<T>() == vector4<T>(0, 0, 0, 0));
	CHECK(vector3<T>::x_axis() == vector3<T>(1, 0, 0) && vector3<T>::axis(2) == vector3<T>(0, 0, 1));
	CHECK(vector4<T>::w_axis() == vector4<T>(0, 0, 0, 1));

	vector3<T> v(1, 2, 3);
	v[1] = 5;
	CHECK(v.y == 5 && v.elements[1] == 5);
	v.set(7);
	CHECK(v == vector3<T>(7, 7, 7));
	const T values[3] = { 4, 5, 6 };
	CHECK(vector3<T>(values) == vector3<T>(4, 5, 6));

	CHECK(vector3<T>(vector2<T>(1, 2), 3) == vector3<T>(1, 2, 3));
	CHECK(vector2<T>(vector3<T>(1, 2, 3)) == vector2<T>(1, 2));
	CHECK(vector4<T>(vector3<T>(1, 2, 3), 4) == vector4<T>(1, 2, 3, 4));
	CHECK(vector4<T>(vector3<T>(1, 2, 3)).w == 0);
	CHECK(vector3<T>(vector4<T>(1, 2, 3, 4)) == vector3<T>(1, 2, 3));
	CHECK(vector4<T>(vector2<T>(1, 2), vector2<T>(3, 4)) == vector4<T>(1, 2, 3, 4));
}

//	@tool 四則演算
MATH_TEST_TYPED(vector_arithmetic)
{
	const vector3<T> a(1, -2, 3);
	const vector3<T> b(4, 5, -6);
	CHECK(a + b == vector3<T>(5, 3, -3));
	CHECK(a - b == vector3<T>(-3, -7, 9));
	CHECK(a * b == vector3<T>(4, -10, -18));
	CHECK(a * static_cast<T>(2) == vector3<T>(2, -4, 6) && static_cast<T>(2) * a == vector3<T>(2, -4, 6));
	CHECK(-a == vector3<T>(-1, 2, -3) && +a == a);
	CHECK_NEAR(b / vector3<T>(2, 5, -3), vector3<T>(2, 1, 2), math_test::tolerance<T>());
	CHECK_NEAR(b / static_cast<T>(2), vector3<T>(2, static_cast<T>(2.5), -3), math_test::tolerance<T>());

	vector3<T> c = a;
	c += b; c -= b; c *= static_cast<T>(3); c /= static_cast<T>(3);
	CHECK_NEAR(c, a, math_test::tolerance<T>());

	const vector4<T> d(1, 2, 3, 4);
	CHECK(d + d == vector4<T>(2, 4, 6, 8) && d * d == vector4<T>(1, 4, 9, 16));
	CHECK(vector2<T>(1, 2) - vector2<T>(3, 5) == vector2<T>(-2, -3));
}

//	@tool SIMD 使用時に未使用要素（vector3 の w、vector2 の z, w）が 0 のまま保たれること
MATH_TEST_TYPED(vector_unused_lanes)
{
	if constexpr (use_simd_v<T>)
	{
		vector3<T> a(1, 2, 3);
		a /= vector3<T>(2, 4, 8);
		CHECK(a.elements[3] == 0);
		a = vector3<T>(vector4<T>(1, 2, 3, 9));
		CHECK(a.elements[3] == 0);
		vector2<T> b(1, 2);
		b /= vector2<T>(4, 8);
		CHECK(b.elements[2] == 0 && b.elements[3] == 0);
		b = vector2<T>(vector4<T>(1, 2, 3, 4));
		CHECK(b.elements[2] == 0 && b.elements[3] == 0);
	}
	else
	{
		CHECK(sizeof(vector3<T>) == sizeof(T) * 3);
	}
}

//	@tool 内積・外積・長さ・正規化
MATH_TEST_TYPED(vector_products)
{
	const double tol = math_test::tolerance<T>();
	const vector3<T> a(1, 2, 3);
	const vector3<T> b(4, 5, 6);
	CHECK_NEAR(vector3<T>::dot(a, b), static_cast<T>(32), tol);
	CHECK_NEAR(a.dot(b), static_cast<T>(32), tol);
	CHECK_NEAR(vector3<T>::cross(a, b), vector3<T>(-3, 6, -3), tol);
	CHECK_NEAR(vector3<T>::cross(vector3<T>::x_axis(), vector3<T>::y_axis()), vector3<T>::z_axis(), tol);
	CHECK_NEAR(vector3<T>(3, 4, 12).length(), static_cast<T>(13), tol);
	CHECK_NEAR(vector3<T>(3, 4, 12).square_length(), static_cast<T>(169), tol);
	CHECK_NEAR(vector3<T>(0, 3, 4).normalized(), vector3<T>(0, static_cast<T>(0.6), static_cast<T>(0.8)), tol);
	CHECK(vector3<T>().normalized() == vector3<T>());

	CHECK_NEAR(vector4<T>(1, 2, 3, 4).dot(vector4<T>(5, 6, 7, 8)), static_cast<T>(70), tol);
	CHECK_NEAR(vector4<T>(1, 1, 1, 1).length(), static_cast<T>(2), tol);
	CHECK_NEAR(vector4<T>(0, 0, 0, 5).normalized(), vector4<T>::w_axis(), tol);

	CHECK_NEAR(vector2<T>(3, 4).length(), static_cast<T>(5), tol);
	CHECK_NEAR(vector2<T>(1, 2).dot(vector2<T>(3, 4)), static_cast<T>(11), tol);
	CHECK_NEAR(vector2<T>::cross(vector2<T>(1, 0), vector2<T>(0, 1)), static_cast<T>(1), tol);
	CHECK_NEAR(vector2<T>(0, 2).normalized(), vector2<T>(0, 1), tol);

	//	@tool 4次元外積：結果は3つの入力すべてと直交する
	const vector4<T> l(1, 2, 0, 1), c(0, 1, 3, 2), r(2, 0, 1, 1);
	const vector4<T> cross4 = vector4<T>::cross(l, c, r);
	CHECK_NEAR(cross4.dot(l), static_cast<T>(0), tol * 10);
	CHECK_NEAR(cross4.dot(c), static_cast<T>(0), tol * 10);
	CHECK_NEAR(cross4.dot(r), static_cast<T>(0), tol * 10);
}

//	@tool 反射・平面に沿ったベクトル
MATH_TEST_TYPED(vector_reflect_scratch)
{
	const double tol = math_test::tolerance<T>();
	const vector3<T> front(1, -1, 0);
	const vector3<T> normal(0, 1, 0);
	CHECK_NEAR(vector3<T>::reflect(front, normal), vector3<T>(1, 1, 0), tol);
	CHECK_NEAR(vector3<T>::scratch(front, normal), vector3<T>(1, 0, 0), tol);
}

//	@tool 行列による変換（規約に関係なく同じ結果になる）
MATH_TEST_TYPED(vector_transform)
{
	const double tol = math_test::tolerance<T>();
	const matrix<T> translation = matrix<T>::translation(10, 20, 30);
	const matrix<T> scaling = matrix<T>::scaling(2, 3, 4);
	const vector3<T> p(1, 2, 3);

	CHECK_NEAR(vector3<T>::transform_coord(p, translation), vector3<T>(11, 22, 33), tol);
	CHECK_NEAR(vector3<T>::transform_normal(p, translation), p, tol);
	CHECK_NEAR(vector3<T>::transform_coord(p, scaling), vector3<T>(2, 6, 12), tol);
	CHECK_NEAR(vector3<T>::transform(p, translation), vector4<T>(11, 22, 33, 1), tol);
	CHECK_NEAR(vector4<T>::transform(vector4<T>(1, 2, 3, 0), translation), vector4<T>(1, 2, 3, 0), tol);
	CHECK_NEAR(vector4<T>::transform(vector4<T>(1, 2, 3, 1), translation), vector4<T>(11, 22, 33, 1), tol);

	//	@tool 拡縮 -> 平行移動 の順
	const matrix<T> scale_then_move = matrix<T>::concatenate(scaling, translation);
	CHECK_NEAR(p.transformed_coord(scale_then_move), vector3<T>(12, 26, 42), tol);
	vector3<T> moved = p;
	moved.transform_coord(scale_then_move);
	CHECK_NEAR(moved, vector3<T>(12, 26, 42), tol);

	//	@tool w による除算（w = 2 になる行列）
	matrix<T> half;
	half.set_storage(1, 0, 0, 0,
					 0, 1, 0, 0,
					 0, 0, 1, 0,
					 0, 0, 0, 2);
	CHECK_NEAR(vector3<T>::transform_coord(p, half), p / static_cast<T>(2), tol);

	//	@tool vector2 は (x, y, 0, 1) として変換する（Z 軸の成分は加算されない）
	matrix<T> z_offset = matrix<T>::translation(0, 0, 0);
	z_offset.storage[2][0] = 5;
	CHECK_NEAR(vector2<T>::transform(vector2<T>(1, 2), z_offset), vector4<T>(1, 2, 0, 1), tol);
	CHECK_NEAR(vector2<T>::transform(vector2<T>(1, 2), translation), vector4<T>(11, 22, 30, 1), tol);
	CHECK_NEAR(vector2<T>::transform_coord(vector2<T>(1, 2), matrix<T>::rotation_z(constants<T>::pi_half)), vector2<T>(-2, 1), tol);
	CHECK_NEAR(vector2<T>::transform_coord(vector2<T>(1, 2), translation), vector2<T>(11, 22), tol);
}

//	@tool ベクトルと行列の積の書き方は規約に従う
MATH_TEST_TYPED(vector_matrix_operator)
{
	const double tol = math_test::tolerance<T>();
	const matrix<T> m = matrix<T>::concatenate(matrix<T>::scaling(2, 3, 4), matrix<T>::translation(1, 2, 3));
	const vector4<T> v(1, 1, 1, 1);
#if MATH_COLUMN_VECTOR
	CHECK_NEAR(m * v, vector4<T>::transform(v, m), tol);
#else
	CHECK_NEAR(v * m, vector4<T>::transform(v, m), tol);
	vector4<T> w = v;
	w *= m;
	CHECK_NEAR(w, vector4<T>(3, 5, 7, 1), tol);
#endif
}

//	@tool 方向の定数（左手系：前方向 +Z、右手系：前方向 -Z）
MATH_TEST_TYPED(vector_direction)
{
	CHECK(vector3<T>::forward(coordinate_order::lh) == vector3<T>(0, 0, 1));
	CHECK(vector3<T>::forward(coordinate_order::rh) == vector3<T>(0, 0, -1));
	CHECK(vector3<T>::forward() == vector3<T>::forward(default_coordinate_order));
	CHECK(vector3<T>::right() == vector3<T>(1, 0, 0) && vector3<T>::up() == vector3<T>(0, 1, 0));
}

//	@tool クォータニオンによる回転
MATH_TEST_TYPED(vector_rotate)
{
	const double tol = math_test::tolerance<T>();
	const quaternion<T> yaw90 = quaternion<T>::rotation_y(constants<T>::pi_half);
	//	@tool Y 軸 90 度回転で +X は -Z に、+Z は +X に向く（行列の rotation_y と同じ向き）
	CHECK_NEAR(vector3<T>(1, 0, 0).rotated(yaw90), vector3<T>(0, 0, -1), tol);
	vector3<T> v(0, 0, 1);
	v.rotate(yaw90);
	CHECK_NEAR(v, vector3<T>(1, 0, 0), tol);
	CHECK_NEAR(vector3<T>(1, 2, 3).rotated(quaternion<T>()), vector3<T>(1, 2, 3), tol);
}

//	@tool 型変換（float <> double、保存用の型）
MATH_TEST_TYPED(vector_conversion)
{
	const vector3<T> v(static_cast<T>(1.25), static_cast<T>(-2.5), static_cast<T>(3.75));
	CHECK(v.to_float3() == float3(1.25f, -2.5f, 3.75f));
	CHECK(v.to_double3() == double3(1.25, -2.5, 3.75));
	CHECK(vector3<T>(v.to_float3()) == v);
	CHECK(vector3<T>(v.to_double3()) == v);
	CHECK(vector3<float>(vector3<double>(1.5, 2.5, 3.5)) == vector3<float>(1.5f, 2.5f, 3.5f));

	const vector4<T> w(1, 2, 3, 4);
	CHECK(w.to_float4() == float4(1, 2, 3, 4) && vector4<T>(w.to_float4()) == w);
	CHECK(vector2<T>(1, 2).to_float2() == float2(1, 2) && vector2<T>(double2(1, 2)) == vector2<T>(1, 2));
}

//	@tool double は遠距離（約 8.8e10 m）でも 0.1 mm 単位の差を保てる
MATH_TEST(vector_large_world_precision)
{
	const vector3d world(8.8e10, -3.0e10, 5.0e10);
	const vector3d object = world + vector3d(0.001, 0.0025, -0.0005);
	CHECK_NEAR(object - world, vector3d(0.001, 0.0025, -0.0005), 2e-5);
	CHECK_NEAR(vector3f(object - world), vector3f(0.001f, 0.0025f, -0.0005f), 2e-5);
	const vector3d moved = vector3d::transform_coord(vector3d(0.001, 0, 0), matrixd::translation(world)) - world;
	CHECK_NEAR(moved.x, 0.001, 2e-5);
}
