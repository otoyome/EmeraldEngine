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
	CHECK(any_greater(vector3<T>(1, 5, 3), vector3<T>(2, 4, 3)) && !any_greater(vector3<T>(1, 4, 3), vector3<T>(2, 4, 3)));
	CHECK(any_less(vector3<T>(1, 5, 3), vector3<T>(2, 4, 3)) && !any_less(vector3<T>(2, 5, 3), vector3<T>(2, 4, 3)));
	CHECK(all_greater(vector2<T>(3, 4), vector2<T>(1, 2)) && !all_greater(vector2<T>(3, 2), vector2<T>(1, 2)));
	CHECK(all_less(vector4<T>(0, 0, 0, 0), vector4<T>(1, 1, 1, 1)) && !all_less(vector4<T>(0, 0, 0, 1), vector4<T>(1, 1, 1, 1)));
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
	//	@tool min_value が 0 以外でも範囲内の値は変わらず、範囲外は端で折り返す
	CHECK(mirror(5, 10, 7) == 7 && mirror(5, 10, 12) == 8 && mirror(5, 10, 3) == 7 && mirror(5, 10, 17) == 7);
	CHECK_NEAR(mirror(static_cast<T>(5), static_cast<T>(10), static_cast<T>(7)), static_cast<T>(7), tol);
	CHECK_NEAR(mirror(static_cast<T>(5), static_cast<T>(10), static_cast<T>(12)), static_cast<T>(8), tol);
	CHECK_NEAR(mirror(static_cast<T>(5), static_cast<T>(10), static_cast<T>(3)), static_cast<T>(7), tol);
	CHECK_NEAR(mirror(static_cast<T>(5), static_cast<T>(10), static_cast<T>(17)), static_cast<T>(7), tol);
	CHECK(between(1, 3, 2) && !between(1, 3, 4));
	CHECK(sign(static_cast<T>(-2)) == -1 && sign(static_cast<T>(0)) == 0 && sign0plus(static_cast<T>(0)) == 1);
	CHECK(math::signbit(static_cast<T>(-0.0)) && !math::signbit(static_cast<T>(1)));

	//	@tool 旧 System/math_utility.h から統合したスカラー版
	CHECK(math::min(static_cast<T>(2), static_cast<T>(-1)) == -1 && math::max(static_cast<T>(2), static_cast<T>(-1)) == 2 && math::min(3, 5) == 3);
	CHECK(clamp(static_cast<T>(5), static_cast<T>(0), static_cast<T>(1)) == 1 && clamp(-3, 0, 10) == 0 && clamp(4, 0, 10) == 4);
	CHECK(saturate(static_cast<T>(-0.5)) == 0 && saturate(static_cast<T>(0.25)) == static_cast<T>(0.25));
	//	@tool clamp_outside は内側の値を近い方の端へ押し出し、外側の値はそのまま
	CHECK(clamp_outside(static_cast<T>(0.3), static_cast<T>(0), static_cast<T>(1)) == 0 && clamp_outside(static_cast<T>(0.7), static_cast<T>(0), static_cast<T>(1)) == 1);
	CHECK(clamp_outside(static_cast<T>(2), static_cast<T>(0), static_cast<T>(1)) == 2);
	CHECK(step(static_cast<T>(1), static_cast<T>(1)) == 1 && step(static_cast<T>(1), static_cast<T>(0.5)) == 0);
	CHECK_NEAR(linearstep(static_cast<T>(2), static_cast<T>(4), static_cast<T>(3)), static_cast<T>(0.5), tol);
	CHECK(linearstep(static_cast<T>(2), static_cast<T>(4), static_cast<T>(5)) == 1);
	CHECK_NEAR(smoothstep(static_cast<T>(0), static_cast<T>(2), static_cast<T>(0.5)), static_cast<T>(0.15625), tol);
	CHECK_NEAR(math::lerp(static_cast<T>(1), static_cast<T>(3), static_cast<T>(0.25)), static_cast<T>(1.5), tol);
	//	@tool 整数の補間は切り捨て
	CHECK(math::lerp(0, 10, static_cast<T>(0.37)) == 3);
	CHECK(nearly_equal(static_cast<T>(1), static_cast<T>(1) + constants<T>::epsilon) && !nearly_equal(static_cast<T>(1), static_cast<T>(1.1)));
	CHECK(nearly_equal(vector3<T>(1, 2, 3), vector3<T>(1, 2, static_cast<T>(3.05)), static_cast<T>(0.1)) && !nearly_equal(vector3<T>(1, 2, 3), vector3<T>(1, 2, static_cast<T>(3.2)), static_cast<T>(0.1)));
	T max_random = static_cast<T>(-2);
	for (int i = 0; i < 100; ++i)
	{
		const T value = rand_range(static_cast<T>(-2), static_cast<T>(3));
		CHECK(value >= -2 && value <= 3);
		max_random = (std::max)(max_random, value);
	}
	//	@tool 範囲の上側まで値が出る（RAND_MAX が小さい環境で範囲の一部しか返さない不具合の確認）
	CHECK(max_random > static_cast<T>(2));
}

//	@tool 整数・ビット系
MATH_TEST(utility_integer)
{
	CHECK(round_up_to_power_of_two(5) == 8 && round_up_to_power_of_two(8) == 8);
	CHECK(calc_next_power_of_two(static_cast<std::uint32_t>(17)) == 32u && calc_next_power_of_two(static_cast<std::uint64_t>(1ull << 40)) == (1ull << 40));
	CHECK(calc_alignment(13, 8) == 16 && calc_alignment(16, 8) == 16);
	CHECK(calc_alignment_power_of_two(13u, 8u) == 16u && calc_alignment_power_of_two(16u, 8u) == 16u && calc_alignment_power_of_two(0u, 16u) == 0u);
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
	//	@tool 正反対の 2 ベクトルでも NaN にならず、直交する軸で回る（途中は単位長で l と直交、両端は l と r）
	for (const vector3<T>& from : { vector3<T>(1, 0, 0), vector3<T>(0, 0, -1), vector3<T>(1, 2, 3).normalized() })
	{
		const vector3<T> to = -from;
		const vector3<T> middle = slerp(from, to, static_cast<T>(0.5));
		CHECK_NEAR(middle.length(), static_cast<T>(1), tol);
		CHECK_NEAR(vector3<T>::dot(middle, from), static_cast<T>(0), tol);
		CHECK_NEAR(slerp(from, to, static_cast<T>(0)), from, tol);
		CHECK_NEAR(slerp(from, to, static_cast<T>(1)), to, tol);
		CHECK_NEAR(vector3<T>::dot(slerp(from, to, static_cast<T>(0.25)), from), std::cos(constants<T>::pi * static_cast<T>(0.25)), tol);
	}
	//	@tool ほぼ同じ向きでも有限の値になる
	{
		const vector3<T> near_x = vector3<T>(1, static_cast<T>(1e-5), 0).normalized();
		const vector3<T> middle = slerp(vector3<T>(1, 0, 0), near_x, static_cast<T>(0.5));
		CHECK(std::isfinite(middle.x) && std::isfinite(middle.y) && std::isfinite(middle.z));
		CHECK_NEAR(middle.length(), static_cast<T>(1), tol);
	}
	CHECK_NEAR(smoothstep(vector3<T>(0, 0, 0), vector3<T>(2, 4, 1), static_cast<T>(1)), vector3<T>(static_cast<T>(0.5), static_cast<T>(0.15625), 1), tol);
	CHECK(bezier(vector2<T>(0, 0), vector2<T>(1, 2), vector2<T>(2, 0), static_cast<T>(0.5)) == vector2<T>(1, 1));
	CHECK_NEAR(bezier(vector3<T>(0, 0, 0), vector3<T>(0, 3, 0), vector3<T>(3, 3, 0), vector3<T>(3, 0, 0), static_cast<T>(0.5)), vector3<T>(static_cast<T>(1.5), static_cast<T>(2.25), 0), tol);
}

//	@tool イージング：始点 0・終点 1
MATH_TEST_TYPED(utility_easing)
{
	const double tol = 1e-5;
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
	//	@tool in は緩やかに始まり、out は緩やかに終わる（in(t) = 1 - out(1 - t)）。in_out は中心対称（in_out(t) = 1 - in_out(1 - t)）
	for (std::int32_t family = 0; family < 10; ++family)
	{
		const easing::type in_type = static_cast<easing::type>(family * 3);
		const easing::type out_type = static_cast<easing::type>(family * 3 + 1);
		const easing::type in_out_type = static_cast<easing::type>(family * 3 + 2);
		for (T time : { static_cast<T>(0.1), static_cast<T>(0.3), static_cast<T>(0.45), static_cast<T>(0.7) })
		{
			CHECK_NEAR(easing::calculate(time, in_type), static_cast<T>(1) - easing::calculate(static_cast<T>(1) - time, out_type), 1e-5);
			CHECK_NEAR(easing::calculate(time, in_out_type), static_cast<T>(1) - easing::calculate(static_cast<T>(1) - time, in_out_type), 1e-5);
		}
		CHECK(easing::calculate(static_cast<T>(0.2), in_type) < static_cast<T>(0.2) + static_cast<T>(1e-5));
	}
	//	@tool すべての曲線が連続している（10 万分割した隣り合う値の差が 0.006 未満。端で接線が垂直になる circ でも約 0.0045）
	for (std::int32_t i = 0; i < static_cast<std::int32_t>(easing::type::lerp); ++i)
	{
		const easing::type type = static_cast<easing::type>(i);
		double max_step = 0;
		for (std::int32_t step = 0; step < 100000; ++step)
		{
			const T a = easing::calculate(static_cast<T>(static_cast<double>(step) / 100000), type);
			const T b = easing::calculate(static_cast<T>(static_cast<double>(step + 1) / 100000), type);
			max_step = std::fmax(max_step, std::fabs(static_cast<double>(b - a)));
		}
		CHECK(max_step < 0.006);
	}
	CHECK_NEAR(easing::in_sine(static_cast<T>(0.5)), static_cast<T>(1) - std::cos(constants<T>::pi * static_cast<T>(0.25)), 1e-6);
	CHECK_NEAR(easing::out_sine(static_cast<T>(0.5)), std::sin(constants<T>::pi * static_cast<T>(0.25)), 1e-6);
	//	@tool circ は単位円の弧：(1 - in_circ(t))^2 + t^2 = 1
	for (T time : { static_cast<T>(0.2), static_cast<T>(0.6), static_cast<T>(0.9) })
	{
		const T c = static_cast<T>(1) - easing::in_circ(time);
		CHECK_NEAR(c * c + time * time, static_cast<T>(1), 1e-5);
	}
	CHECK(easing::out_expo(static_cast<T>(1)) == static_cast<T>(1) && easing::in_out_expo(static_cast<T>(1)) == static_cast<T>(1));
	CHECK(easing::in_expo(static_cast<T>(0)) == static_cast<T>(0) && easing::in_out_expo(static_cast<T>(0)) == static_cast<T>(0));
	//	@tool easings.net の式と同じ値
	CHECK_NEAR(easing::in_expo(static_cast<T>(0.5)), static_cast<T>(0.03125), 1e-6);
	CHECK_NEAR(easing::in_out_expo(static_cast<T>(0.25)), static_cast<T>(0.015625), 1e-6);
	CHECK_NEAR(easing::in_elastic(static_cast<T>(0.5)), static_cast<T>(-0.015625), 1e-6);
	CHECK_NEAR(easing::in_out_elastic(static_cast<T>(0.5)), static_cast<T>(0.5), 1e-6);
	CHECK_NEAR(easing::out_bounce(static_cast<T>(0.5)), static_cast<T>(0.765625), 1e-6);
	CHECK_NEAR(easing::in_out_bounce(static_cast<T>(0.5)), static_cast<T>(0.5), 1e-6);
	CHECK_NEAR(easing::in_out_back(static_cast<T>(0.25)), static_cast<T>(-0.0996818437), 1e-6);
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
	//	@tool 範囲内の値は丸め誤差なしでそのまま返す（毎フレーム正規化しても値がずれない）
	CHECK(angle::normalize(static_cast<T>(0.3)) == static_cast<T>(0.3) && angle::normalize(static_cast<T>(-2.5)) == static_cast<T>(-2.5));
	CHECK_NEAR(angle::normalize(static_cast<T>(-7)), static_cast<T>(-7) + constants<T>::pi2, tol * 10);
	CHECK_NEAR(angle::normalize_positive(static_cast<T>(-0.5)), constants<T>::pi2 - static_cast<T>(0.5), tol * 10);
	CHECK_NEAR(angle::normalize_positive(constants<T>::pi2 * 3 + static_cast<T>(1)), static_cast<T>(1), tol * 100);
	CHECK(angle::normalize_positive(static_cast<T>(0)) == 0 && angle::normalize_positive(static_cast<T>(1)) == static_cast<T>(1));
	for (T value : { static_cast<T>(-1e-9), static_cast<T>(-100), static_cast<T>(57) })
	{
		const T positive = angle::normalize_positive(value);
		CHECK(positive >= 0 && positive < constants<T>::pi2);
	}
	CHECK(angle::is_sharp_angle(vector3<T>(1, 0, 0), vector3<T>(), vector3<T>(1, 1, 0)));
	CHECK(!angle::is_sharp_angle(vector3<T>(1, 0, 0), vector3<T>(), vector3<T>(-1, 1, 0)));
	//	@tool 垂直視野角 90 度・アスペクト比 2 の水平視野角は 2 * atan(2)（横長なら水平視野角の方が広い）
	const T fov = static_cast<T>(1.1), aspect = static_cast<T>(1.6);
	CHECK_NEAR(angle::fov_vertical_to_fov_horizontal(constants<T>::pi_half, static_cast<T>(2)), static_cast<T>(2) * std::atan(static_cast<T>(2)), tol);
	CHECK_NEAR(angle::fov_horizontal_to_fov_vertical(static_cast<T>(2) * std::atan(static_cast<T>(2)), static_cast<T>(2)), constants<T>::pi_half, tol);
	CHECK(angle::fov_vertical_to_fov_horizontal(fov, aspect) > fov);
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
