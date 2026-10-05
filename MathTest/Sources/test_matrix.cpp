#include "test_framework.h"

using namespace math;

namespace
{
	//	@tool 行列同士の比較用（要素ごとの差）
	template<typename T>
	matrix<T> storage_matrix(T s11, T s12, T s13, T s14,
							 T s21, T s22, T s23, T s24,
							 T s31, T s32, T s33, T s34,
							 T s41, T s42, T s43, T s44)
	{
		matrix<T> ret;
		ret.set_storage(s11, s12, s13, s14, s21, s22, s23, s24, s31, s32, s33, s34, s41, s42, s43, s44);
		return ret;
	}
}

//	@tool 単位行列・格納・平行移動
MATH_TEST_TYPED(matrix_basic)
{
	const matrix<T> identity;
	for (std::uint32_t i = 0; i < 16; ++i)
	{
		CHECK(identity.elements[i] == ((i % 5 == 0) ? 1 : 0));
	}

	const matrix<T> t = matrix<T>::translation(1, 2, 3);
	//	@tool 格納は規約に関係なく平行移動が storage[3]（4 番目の軸ベクトル）に入る
	CHECK(t.storage[3][0] == 1 && t.storage[3][1] == 2 && t.storage[3][2] == 3 && t.storage[3][3] == 1);
	CHECK(t.get_translation() == vector3<T>(1, 2, 3));
	CHECK(t.to_vector3(3) == vector3<T>(1, 2, 3) && t.to_vector4(0) == vector4<T>(1, 0, 0, 0));

	matrix<T> u;
	u.set_translation(vector3<T>(4, 5, 6));
	CHECK(u.get_translation() == vector3<T>(4, 5, 6));
	CHECK(matrix<T>::translation(vector2<T>(1, 2)).get_translation() == vector3<T>(1, 2, 0));
	CHECK(matrix<T>::translation(vector4<T>(1, 2, 3, 4)).storage[3][3] == 4);

	const matrix<T> axes(vector4<T>(1, 2, 3, 0), vector4<T>(4, 5, 6, 0), vector4<T>(7, 8, 9, 0), vector4<T>(10, 11, 12, 1));
	CHECK(axes.storage[1][2] == 6 && axes.storage[3][0] == 10);
	CHECK(axes == storage_matrix<T>(1, 2, 3, 0, 4, 5, 6, 0, 7, 8, 9, 0, 10, 11, 12, 1));
}

//	@tool 数学上の要素名・(行, 列)・16 要素の生成は規約に従う
MATH_TEST_TYPED(matrix_element_naming)
{
	const matrix<T> t = matrix<T>::translation(1, 2, 3);
#if MATH_COLUMN_VECTOR
	CHECK(t._14 == 1 && t._24 == 2 && t._34 == 3 && t._41 == 0);
	CHECK(t(0, 3) == 1 && t(2, 3) == 3);
	CHECK(matrix<T>(1, 0, 0, 1,  0, 1, 0, 2,  0, 0, 1, 3,  0, 0, 0, 1) == t);
#else
	CHECK(t._41 == 1 && t._42 == 2 && t._43 == 3 && t._14 == 0);
	CHECK(t(3, 0) == 1 && t(3, 2) == 3);
	CHECK(matrix<T>(1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  1, 2, 3, 1) == t);
#endif
	//	@tool 格納名 s11～s44 は規約に関係なく storage と同じ位置
	CHECK(t.s41 == 1 && t.s43 == 3 && t.storage[3][2] == t.s43);
	matrix<T> m;
	m(1, 2) = 7;
	CHECK(m._23 == 7);
}

//	@tool 積の順序（A * B は規約に従い、concatenate は規約に関係なく「first の後に second」）
MATH_TEST_TYPED(matrix_multiply)
{
	const double tol = math_test::tolerance<T>();
	const matrix<T> s = matrix<T>::scaling(2, 2, 2);
	const matrix<T> t = matrix<T>::translation(1, 0, 0);
	const vector3<T> p(1, 1, 1);

	//	@tool 拡縮の後に平行移動 = (3, 2, 2)、平行移動の後に拡縮 = (4, 2, 2)
	CHECK_NEAR(vector3<T>::transform_coord(p, matrix<T>::concatenate(s, t)), vector3<T>(3, 2, 2), tol);
	CHECK_NEAR(vector3<T>::transform_coord(p, matrix<T>::concatenate(t, s)), vector3<T>(4, 2, 2), tol);
#if MATH_COLUMN_VECTOR
	CHECK(t * s == matrix<T>::concatenate(s, t));
#else
	CHECK(s * t == matrix<T>::concatenate(s, t));
#endif
	matrix<T> m = s;
	m *= t;
	CHECK(m == s * t);
	CHECK(s * matrix<T>() == s && matrix<T>() * s == s);

	//	@tool 結合法則
	const matrix<T> r = matrix<T>::rotation_y(static_cast<T>(0.7));
	CHECK_NEAR((s * t) * r, s * (t * r), tol);

	CHECK(matrix<T>::scaling(1, 2, 3) + matrix<T>::scaling(1, 1, 1) == matrix<T>::scaling(2, 3, 4) + matrix<T>(0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 1));
	CHECK_NEAR(matrix<T>::scaling(2, 4, 6) / static_cast<T>(2), storage_matrix<T>(1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, static_cast<T>(0.5)), tol);
}

//	@tool 回転行列の向き（行ベクトル・左手系で設計。規約が変わっても同じ回転を表す）
MATH_TEST_TYPED(matrix_rotation)
{
	const double tol = math_test::tolerance<T>();
	const T half_pi = constants<T>::pi_half;
	//	@tool X 軸 90 度：+Y -> +Z、Y 軸 90 度：+Z -> +X、Z 軸 90 度：+X -> +Y
	CHECK_NEAR(vector3<T>::transform_normal(vector3<T>(0, 1, 0), matrix<T>::rotation_x(half_pi)), vector3<T>(0, 0, 1), tol);
	CHECK_NEAR(vector3<T>::transform_normal(vector3<T>(0, 0, 1), matrix<T>::rotation_y(half_pi)), vector3<T>(1, 0, 0), tol);
	CHECK_NEAR(vector3<T>::transform_normal(vector3<T>(1, 0, 0), matrix<T>::rotation_z(half_pi)), vector3<T>(0, 1, 0), tol);

	//	@tool オイラー角の回転順序（xyz：X 軸回転の後に Y 軸回転の後に Z 軸回転）
	const T x = static_cast<T>(0.4), y = static_cast<T>(-0.7), z = static_cast<T>(1.2);
	const matrix<T> rx = matrix<T>::rotation_x(x), ry = matrix<T>::rotation_y(y), rz = matrix<T>::rotation_z(z);
	CHECK_NEAR(matrix<T>::rotation_xyz(x, y, z, rotation_order::xyz), matrix<T>::concatenate(matrix<T>::concatenate(rx, ry), rz), tol);
	CHECK_NEAR(matrix<T>::rotation_xyz(x, y, z, rotation_order::xzy), matrix<T>::concatenate(matrix<T>::concatenate(rx, rz), ry), tol);
	CHECK_NEAR(matrix<T>::rotation_xyz(x, y, z, rotation_order::yxz), matrix<T>::concatenate(matrix<T>::concatenate(ry, rx), rz), tol);
	CHECK_NEAR(matrix<T>::rotation_xyz(x, y, z, rotation_order::yzx), matrix<T>::concatenate(matrix<T>::concatenate(ry, rz), rx), tol);
	CHECK_NEAR(matrix<T>::rotation_xyz(x, y, z, rotation_order::zxy), matrix<T>::concatenate(matrix<T>::concatenate(rz, rx), ry), tol);
	CHECK_NEAR(matrix<T>::rotation_xyz(x, y, z, rotation_order::zyx), matrix<T>::concatenate(matrix<T>::concatenate(rz, ry), rx), tol);
	CHECK(matrix<T>::rotation_xyz(vector3<T>(x, y, z)) == matrix<T>::rotation_xyz(x, y, z, default_rotation_order));

	//	@tool 軸回転とクォータニオンから作った行列が一致する
	const vector3<T> axis = vector3<T>(1, 2, 3).normalized();
	CHECK_NEAR(matrix<T>::rotation_axis(axis, static_cast<T>(0.9)), matrix<T>(quaternion<T>::rotation_axis(axis, static_cast<T>(0.9))), tol);
	CHECK_NEAR(matrix<T>::rotation_quaternion(quaternion<T>::rotation_x(x)), rx, tol);
}

//	@tool 転置・行列式・逆行列
MATH_TEST_TYPED(matrix_inverse)
{
	const double tol = std::is_same_v<T, float> ? 1e-3 : 1e-9;
	const matrix<T> m = storage_matrix<T>(2, 0, 1, 0,
										  1, 3, 0, 0,
										  0, 1, 4, 0,
										  5, 6, 7, 1);
	CHECK(m.transposed().transposed() == m);
	CHECK(m.transposed().storage[0][1] == m.storage[1][0]);
	CHECK_NEAR(matrix<T>::scaling(2, 3, 4).determinant(), static_cast<T>(24), tol);
	CHECK_NEAR(matrix<T>::rotation_y(static_cast<T>(0.3)).determinant(), static_cast<T>(1), tol);
	//	@tool 2 * (3 * 4 - 0) - 0 + 1 * (1 * 1 - 0) = 25
	CHECK_NEAR(m.determinant(), static_cast<T>(25), tol);
	CHECK_NEAR(m * m.inverted(), matrix<T>(), tol);
	CHECK_NEAR(m.inverted() * m, matrix<T>(), tol);
	matrix<T> n = m;
	n.invert();
	CHECK(n == m.inverted());

	//	@tool 行列式が 0 のときは元の行列を返す
	const matrix<T> singular = matrix<T>::scaling(1, 0, 1);
	CHECK(singular.inverted() == singular);
	//	@tool try_inverse は逆行列が存在しないと false を返し、out を変更しない
	matrix<T> inverse_out = matrix<T>::translation(1, 2, 3);
	CHECK(!matrix<T>::try_inverse(singular, inverse_out));
	CHECK(inverse_out == matrix<T>::translation(1, 2, 3));
	CHECK(matrix<T>::try_inverse(m, inverse_out));
	CHECK_NEAR(inverse_out, m.inverted(), tol);

	const matrix<T> rt = matrix<T>::concatenate(matrix<T>::rotation_xyz(static_cast<T>(0.3), static_cast<T>(1.1), static_cast<T>(-0.4)), matrix<T>::translation(5, -6, 7));
	CHECK_NEAR(matrix<T>::inverse_for_rt(rt), rt.inverted(), tol);
	CHECK_NEAR(matrix<T>::inverse_for_rotation(matrix<T>::rotation_z(static_cast<T>(0.5))), matrix<T>::rotation_z(static_cast<T>(-0.5)), tol);
	CHECK_NEAR(matrix<T>::inverse_for_translation(matrix<T>::translation(1, 2, 3)), matrix<T>::translation(-1, -2, -3), tol);
	CHECK_NEAR(matrix<T>::inverse_for_scaling(matrix<T>::scaling(2, 4, 8)), matrix<T>::scaling(static_cast<T>(0.5), static_cast<T>(0.25), static_cast<T>(0.125)), tol);
}

//	@tool 姿勢行列の生成と分解
MATH_TEST_TYPED(matrix_transform_decompose)
{
	const double tol = std::is_same_v<T, float> ? 1e-3 : 1e-9;
	const vector3<T> scale(2, 3, 4);
	const quaternion<T> rotation = quaternion<T>::rotation_axis(vector3<T>(1, -2, 0.5).normalized(), static_cast<T>(1.3));
	const vector3<T> position(10, -20, 30);
	const matrix<T> srt = matrix<T>::transform(scale, rotation, position);

	//	@tool 拡縮 -> 回転 -> 平行移動
	const vector3<T> p(1, 1, 1);
	CHECK_NEAR(vector3<T>::transform_coord(p, srt), (p * scale).rotated(rotation) + position, tol);
	CHECK_NEAR(srt, matrix<T>::concatenate(matrix<T>::concatenate(matrix<T>::scaling(scale), matrix<T>(rotation)), matrix<T>::translation(position)), tol);
	CHECK_NEAR(matrix<T>(rotation, position), matrix<T>::concatenate(matrix<T>(rotation), matrix<T>::translation(position)), tol);

	const vector3<T> euler(static_cast<T>(0.2), static_cast<T>(-0.5), static_cast<T>(0.9));
	CHECK_NEAR(matrix<T>::transform(scale, euler, position), matrix<T>::transform(scale, quaternion<T>(euler), position), tol);

	vector3<T> out_scale, out_position;
	quaternion<T> out_rotation;
	CHECK(srt.decompose(out_scale, out_rotation, out_position));
	CHECK_NEAR(out_scale, scale, tol);
	CHECK_NEAR(out_position, position, tol);
	CHECK_SAME_ROTATION(out_rotation, rotation, tol);

	//	@tool 拡縮が 0 の軸があっても分解できる（退化した軸の向きは決められないため符号は問わない）
	CHECK(matrix<T>::transform(vector3<T>(2, 0, 1), rotation, position).decompose(out_scale, out_rotation, out_position));
	CHECK_NEAR(math::abs(out_scale), vector3<T>(2, 0, 1), tol);
	CHECK_NEAR(out_position, position, tol);
}

//	@tool ミラーリング
MATH_TEST_TYPED(matrix_mirror)
{
	const double tol = math_test::tolerance<T>();
	//	@tool X 軸ミラーでは Y 軸回転の向きが反転する
	const matrix<T> r = matrix<T>::rotation_y(static_cast<T>(0.6));
	CHECK_NEAR(r.mirrored(mirroring_order::x), matrix<T>::rotation_y(static_cast<T>(-0.6)), tol);
	matrix<T> m = r;
	m.mirror(mirroring_order::x);
	CHECK(m == r.mirrored(mirroring_order::x));
	CHECK_NEAR(matrix<T>::rotation_x(static_cast<T>(0.6)).mirrored(mirroring_order::x), matrix<T>::rotation_x(static_cast<T>(0.6)), tol);
}

//	@tool 型変換（格納順のままコピーする）
MATH_TEST_TYPED(matrix_conversion)
{
	const matrix<T> m = matrix<T>::transform(vector3<T>(1, 2, 3), quaternion<T>::rotation_z(static_cast<T>(0.5)), vector3<T>(7, 8, 9));
	const float4x4 f = m.to_float4x4();
	for (std::uint32_t i = 0; i < 16; ++i)
	{
		CHECK(f.elements[i] == static_cast<float>(m.elements[i]));
	}
	CHECK(f.m[3][0] == 7.0f && f.m[3][2] == 9.0f);
	const double4x4 d = m.to_double4x4();
	CHECK(d.m[1][1] == static_cast<double>(m.storage[1][1]));
	const float4x3 c = m.to_float4x3();
	CHECK(c.m[3][0] == 7.0f && c.m[3][2] == 9.0f && c.m[1][1] == static_cast<float>(m.storage[1][1]));
	CHECK(matrix<T>(element_matrix<T, 4, 4>(m.to_double4x4())) == m);
	CHECK(matrix<float>(matrix<double>::translation(1, 2, 3)) == matrix<float>::translation(1, 2, 3));
	CHECK(float4x4().m[0][0] == 1 && float4x4().m[0][1] == 0);
}

//	@tool 格納は規約（行 / 列ベクトル）に関係なく「左手系・行ベクトル規約の行列を行優先で並べた形」になる
MATH_TEST_TYPED(matrix_storage_layout)
{
	const double tol = math_test::tolerance<T>();
	const T angle = static_cast<T>(0.5);
	const matrix<T> rx = matrix<T>::rotation_x(angle);
	CHECK_NEAR(rx.s22, std::cos(angle), tol);
	CHECK_NEAR(rx.s23, std::sin(angle), tol);
	CHECK_NEAR(rx.s32, -std::sin(angle), tol);

	//	@tool 左手系・リバースデプスの透視投影
	const T near_z = 1, far_z = 101;
	const matrix<T> projection = matrix<T>::perspective_fov(constants<T>::pi_half, 1, near_z, far_z, coordinate_order::lh, projection_z_order::reverse);
	CHECK_NEAR(projection.s11, static_cast<T>(1), tol);
	CHECK_NEAR(projection.s22, static_cast<T>(1), tol);
	CHECK_NEAR(projection.s33, -near_z / (far_z - near_z), tol);
	CHECK_NEAR(projection.s34, static_cast<T>(1), tol);
	CHECK_NEAR(projection.s43, far_z * near_z / (far_z - near_z), tol);
	CHECK_NEAR(projection.s44, static_cast<T>(0), tol);

	//	@tool 左手系の視線行列（-Z 側から原点を見る）
	const matrix<T> view = matrix<T>::look_at(vector3<T>(0, 0, -5), vector3<T>(), vector3<T>::up(), coordinate_order::lh);
	CHECK_NEAR(view, matrix<T>::translation(0, 0, 5), tol);
	CHECK_NEAR(view.s43, static_cast<T>(5), tol);
}