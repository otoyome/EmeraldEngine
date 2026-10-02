#include "test_framework.h"

using namespace math;

//	@tool ベクトルの min / max / clamp / saturate / abs
MATH_TEST_TYPED(utility_vector_elementwise)
{
	CHECK((min)(vector3<T>(1, 5, 3), vector3<T>(2, 4, 3)) == vector3<T>(1, 4, 3));
	CHECK((max)(vector3<T>(1, 5, 3), vector3<T>(2, 4, 3)) == vector3<T>(2, 5, 3));
	CHECK(clamp(vector3<T>(-1, static_cast<T>(0.5), 2), vector3<T>(0, 0, 0), vector3<T>(1, 1, 1)) == vector3<T>(0, static_cast<T>(0.5), 1));
	CHECK(saturate(vector4<T>(-1, static_cast<T>(0.25), 2, 1)) == vector4<T>(0, static_cast<T>(0.25), 1, 1));
	CHECK(saturate(vector2<T>(-1, 2)) == vector2<T>(0, 1));
	CHECK(math::abs(vector3<T>(-1, 2, -3)) == vector3<T>(1, 2, 3));
	CHECK(math::abs(vector2<T>(-1, -2)) == vector2<T>(1, 2));
	CHECK(math::abs(-3) == 3 && math::abs(static_cast<T>(-1.5)) == static_cast<T>(1.5));
	CHECK(element_max(vector3<T>(1, 7, 3)) == 7 && element_min(vector4<T>(4, 2, 9, 1)) == 1);
}

//	@tool ループ・反復・範囲・符号
MATH_TEST_TYPED(utility_scalar)
{
	const double tol = math_test::tolerance<T>();
	CHECK_NEAR(repeat(static_cast<T>(10), static_cast<T>(12)), static_cast<T>(2), tol);
	CHECK_NEAR(repeat(static_cast<T>(10), static_cast<T>(-3)), static_cast<T>(7), tol);
	CHECK(repeat(10, -3) == 7);
	CHECK_NEAR(wrap(static_cast<T>(5), static_cast<T>(10), static_cast<T>(12)), static_cast<T>(7), tol);
	CHECK_NEAR(wrap(vector3<T>(0, 0, 0), vector3<T>(10, 10, 10), vector3<T>(12, -3, 5)), vector3<T>(2, 7, 5), tol);
	CHECK_NEAR(mirror(static_cast<T>(0), static_cast<T>(10), static_cast<T>(13)), static_cast<T>(7), tol);
	CHECK(mirror(0, 10, 13) == 7);
	CHECK(between(1, 3, 2) && !between(1, 3, 4));
	CHECK(sign(static_cast<T>(-2)) == -1 && sign(static_cast<T>(0)) == 0 && sign0plus(static_cast<T>(0)) == 1);
	CHECK(math::signbit(static_cast<T>(-0.0)) && !math::signbit(static_cast<T>(1)));
}

//	@tool 整数・ビット系
MATH_TEST(utility_integer)
{
	CHECK(round_up_to_power_of_two(5) == 8 && round_up_to_power_of_two(8) == 8);
	CHECK(calc_next_power_of_two(static_cast<std::uint32_t>(17)) == 32u && calc_next_power_of_two(static_cast<std::uint64_t>(1ull << 40)) == (1ull << 40));
	CHECK(calc_alignment(13, 8) == 16 && calc_alignment(16, 8) == 16);
	CHECK(calc_digit(12345) == 5 && calc_digit(0) == 0 && calc_digit(0, true) == 1);
	CHECK(convert_endian(static_cast<std::uint32_t>(0x11223344u)) == 0x44332211u);
	CHECK(convert_endian(static_cast<std::uint16_t>(0x1122u)) == 0x2211u);
	CHECK(convert_endian(static_cast<std::uint64_t>(0x0102030405060708ull)) == 0x0807060504030201ull);
	CHECK(convert_endian(static_cast<std::int32_t>(1)) == 0x01000000);
	static_assert(calc_next_power_of_two(static_cast<std::uint32_t>(3)) == 4u, "constexpr");
	CHECK(second_to_millisecond(1.5) == 1500.0 && millimeter_to_meter(250.0) == 0.25);
	CHECK(std::fabs(approx_sqrt(16.0f) - 4.0f) < 0.05f);
}

//	@tool 三角形の面積
MATH_TEST_TYPED(utility_triangle_area)
{
	const double tol = math_test::tolerance<T>();
	CHECK_NEAR(calculate_triangle_area(vector2<T>(0, 0), vector2<T>(4, 0), vector2<T>(0, 3)), static_cast<T>(6), tol);
	CHECK_NEAR(calculate_triangle_area(vector3<T>(0, 0, 0), vector3<T>(0, 4, 0), vector3<T>(0, 0, 3)), static_cast<T>(6), tol);
}

//	@tool 補間
MATH_TEST_TYPED(utility_interpolation)
{
	const double tol = math_test::tolerance<T>();
	CHECK(lerp(vector3<T>(0, 0, 0), vector3<T>(2, 4, 6), static_cast<T>(0.5)) == vector3<T>(1, 2, 3));
	CHECK(lerp(vector2<T>(0, 0), vector2<T>(2, 4), static_cast<T>(0.25)) == vector2<T>(static_cast<T>(0.5), 1));
	CHECK(lerp(vector4<T>(0, 0, 0, 0), vector4<T>(4, 4, 4, 4), static_cast<T>(0.5)) == vector4<T>(2, 2, 2, 2));
	CHECK_NEAR(slerp(vector3<T>(1, 0, 0), vector3<T>(0, 1, 0), static_cast<T>(0.5)), vector3<T>(1, 1, 0).normalized(), tol);
	CHECK_NEAR(slerp(vector3<T>(1, 0, 0), vector3<T>(1, 0, 0), static_cast<T>(0.5)), vector3<T>(1, 0, 0), tol);
	CHECK_NEAR(smoothstep(vector3<T>(0, 0, 0), vector3<T>(2, 4, 1), static_cast<T>(1)), vector3<T>(static_cast<T>(0.5), static_cast<T>(0.15625), 1), tol);
	CHECK(bezier(vector2<T>(0, 0), vector2<T>(1, 2), vector2<T>(2, 0), static_cast<T>(0.5)) == vector2<T>(1, 1));
	CHECK_NEAR(bezier(vector3<T>(0, 0, 0), vector3<T>(0, 3, 0), vector3<T>(3, 3, 0), vector3<T>(3, 0, 0), static_cast<T>(0.5)), vector3<T>(static_cast<T>(1.5), static_cast<T>(2.25), 0), tol);
}

//	@tool イージング：始点 0・終点 1（指数系は定義上 1 / 255 程度の誤差がある）
MATH_TEST_TYPED(utility_easing)
{
	const double tol = 5e-3;
	for (std::int32_t i = 0; i < static_cast<std::int32_t>(easing::type::lerp); ++i)
	{
		const easing::type type = static_cast<easing::type>(i);
		CHECK_NEAR(easing::calculate(static_cast<T>(0), type), static_cast<T>(0), tol);
		CHECK_NEAR(easing::calculate(static_cast<T>(1), type), static_cast<T>(1), tol);
		//	@tool 範囲外は 0～1 に丸めてから計算する
		CHECK_NEAR(easing::calculate(static_cast<T>(2), type), easing::calculate(static_cast<T>(1), type), 1e-9);
		CHECK_NEAR(easing::calculate(static_cast<T>(-1), type), easing::calculate(static_cast<T>(0), type), 1e-9);
	}
	CHECK_NEAR(easing::in_quad(static_cast<T>(0.5)), static_cast<T>(0.25), 1e-6);
	CHECK_NEAR(easing::in_out_cubic(static_cast<T>(0.75)), static_cast<T>(0.9375), 1e-5);
	CHECK_NEAR(easing::calculate(static_cast<T>(0.3), easing::type::lerp), static_cast<T>(0.3), 1e-6);
}

//	@tool 角度
MATH_TEST_TYPED(utility_angle)
{
	const double tol = math_test::tolerance<T>();
	CHECK_NEAR(angle::to_radian(static_cast<T>(180)), constants<T>::pi, tol);
	CHECK_NEAR(angle::to_degree(constants<T>::pi_half), static_cast<T>(90), tol * 100);
	CHECK_NEAR(angle::to_radian(vector3<T>(90, 180, 0)), vector3<T>(constants<T>::pi_half, constants<T>::pi, 0), tol);
	//	@tool ±π ちょうどは丸め誤差でどちらにもなりうるため、少しずらした値で確認する
	CHECK_NEAR(angle::normalize(constants<T>::pi * 3 - static_cast<T>(0.5)), constants<T>::pi - static_cast<T>(0.5), tol * 10);
	CHECK_NEAR(angle::difference(static_cast<T>(0.1), constants<T>::pi2 - static_cast<T>(0.1)), static_cast<T>(0.2), tol * 10);
	CHECK(angle::is_sharp_angle(vector3<T>(1, 0, 0), vector3<T>(), vector3<T>(1, 1, 0)));
	CHECK(!angle::is_sharp_angle(vector3<T>(1, 0, 0), vector3<T>(), vector3<T>(-1, 1, 0)));
	//	@tool fov_vertical_to_fov_horizontal と fov_horizontal_to_fov_vertical は互いに逆変換（Framework の名前どおり）
	const T fov = static_cast<T>(1.1), aspect = static_cast<T>(1.6);
	CHECK_NEAR(angle::fov_vertical_to_fov_horizontal(angle::fov_horizontal_to_fov_vertical(fov, aspect), aspect), fov, tol);
	for (T a : { static_cast<T>(-7), static_cast<T>(-1), static_cast<T>(0), static_cast<T>(0.5), static_cast<T>(2), static_cast<T>(10) })
	{
		T s, c;
		angle::sin_cos(a, s, c);
		CHECK_NEAR(s, std::sin(a), 2e-6);
		CHECK_NEAR(c, std::cos(a), 2e-6);
	}
}

//	@tool 保存用の型
MATH_TEST(utility_element)
{
	CHECK(sizeof(float2) == 8 && sizeof(float3) == 12 && sizeof(float4) == 16 && sizeof(int3) == 12 && sizeof(ushort4) == 8);
	CHECK(sizeof(float4x4) == 64 && sizeof(float4x3) == 48 && sizeof(double4x4) == 128);
	CHECK(float3(1, 2, 3)[1] == 2 && float3(1, 2, 3) != float3(1, 2, 4));
	CHECK(double3(float3(1.5f, 2.5f, 3.5f)) == double3(1.5, 2.5, 3.5));
	CHECK(int2(float2(1.9f, -1.9f)) == int2(1, -1));
	float3x3 m;
	CHECK(m(0, 0) == 1 && m(1, 0) == 0 && m(2, 2) == 1);
	m(1, 2) = 5;
	CHECK(m.m[1][2] == 5);
	CHECK(float3x4().m[2][2] == 1 && float3x4().m[2][3] == 0);
}
