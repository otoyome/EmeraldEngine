#include "test_framework.h"

using namespace math;

namespace
{
	//	@tool SIMD レジスタの中身を取り出す
	template<typename Ops>
	void store(const typename Ops::reg& v, typename Ops::value_type out[4])
	{
		union
		{
			typename Ops::reg r;
			typename Ops::value_type e[4];
		} u;
		u.r = v;
		for (int i = 0; i < 4; ++i)
		{
			out[i] = u.e[i];
		}
	}

	template<typename Ops>
	bool equals(const typename Ops::reg& v, double x, double y, double z, double w, double allowed = 1e-5)
	{
		typename Ops::value_type e[4];
		store<Ops>(v, e);
		return std::fabs(e[0] - x) <= allowed && std::fabs(e[1] - y) <= allowed && std::fabs(e[2] - z) <= allowed && std::fabs(e[3] - w) <= allowed;
	}

	//	@tool f4（float / SSE）と d4（double / SSE2 または AVX2）の共通テスト
	template<typename Ops>
	void test_ops()
	{
		using reg = typename Ops::reg;
		const reg a = Ops::set(1, 2, 3, 4);
		const reg b = Ops::set(5, -6, 7, -8);

		CHECK(equals<Ops>(Ops::add(a, b), 6, -4, 10, -4));
		CHECK(equals<Ops>(Ops::sub(a, b), -4, 8, -4, 12));
		CHECK(equals<Ops>(Ops::mul(a, b), 5, -12, 21, -32));
		CHECK(equals<Ops>(Ops::div(b, a), 5, -3, 7.0 / 3.0, -2));
		CHECK(equals<Ops>(Ops::min(a, b), 1, -6, 3, -8) && equals<Ops>(Ops::max(a, b), 5, 2, 7, 4));
		CHECK(equals<Ops>(Ops::abs(b), 5, 6, 7, 8) && equals<Ops>(Ops::neg(a), -1, -2, -3, -4));
		CHECK(equals<Ops>(Ops::sqrt(Ops::set(4, 9, 16, 25)), 2, 3, 4, 5));
		CHECK(equals<Ops>(Ops::set1(3), 3, 3, 3, 3) && equals<Ops>(Ops::zero(), 0, 0, 0, 0));
		CHECK(Ops::first(a) == 1);

		CHECK(equals<Ops>(Ops::template permute<3, 2, 1, 0>(a), 4, 3, 2, 1));
		CHECK(equals<Ops>(Ops::template permute<1, 1, 3, 0>(a), 2, 2, 4, 1));
		CHECK(equals<Ops>(Ops::template shuffle<3, 0, 1, 2>(a, b), 4, 1, -6, 7));
		CHECK(equals<Ops>(Ops::template splat<2>(a), 3, 3, 3, 3));
		CHECK(equals<Ops>(Ops::mask_xyz(a), 1, 2, 3, 0) && equals<Ops>(Ops::mask_xy(a), 1, 2, 0, 0));
		CHECK(equals<Ops>(Ops::select(Ops::cmp_lt(a, Ops::set1(3)), a, b), 1, 2, 7, -8));
		CHECK(equals<Ops>(Ops::select(Ops::cmp_ge(a, Ops::set1(3)), a, b), 5, -6, 3, 4));
		CHECK(equals<Ops>(Ops::select(Ops::cmp_neq(a, Ops::set(1, 0, 3, 0)), a, b), 5, 2, 7, 4));

		CHECK(equals<Ops>(Ops::dot2(a, b), -7, -7, -7, -7));
		CHECK(equals<Ops>(Ops::dot3(a, b), 14, 14, 14, 14));
		CHECK(equals<Ops>(Ops::dot4(a, b), -18, -18, -18, -18));
		CHECK(equals<Ops>(simd::cross3<Ops>(Ops::set(1, 0, 0, 0), Ops::set(0, 1, 0, 0)), 0, 0, 1, 0));
		CHECK(equals<Ops>(simd::normalize<Ops>(Ops::set(0, 3, 4, 0), Ops::dot4(Ops::set(0, 3, 4, 0), Ops::set(0, 3, 4, 0)), static_cast<typename Ops::value_type>(1e-6)), 0, 0.6, 0.8, 0));

		//	@tool float 4 要素との相互変換
		CHECK(equals<Ops>(Ops::from_float4(_mm_setr_ps(1, 2, 3, 4)), 1, 2, 3, 4));
		float f[4];
		_mm_storeu_ps(f, Ops::to_float4(b));
		CHECK(f[0] == 5 && f[1] == -6 && f[2] == 7 && f[3] == -8);

		//	@tool 行列演算（行列式 25 の行列）
		reg m[4] = { Ops::set(2, 0, 1, 0), Ops::set(1, 3, 0, 0), Ops::set(0, 1, 4, 0), Ops::set(5, 6, 7, 1) };
		reg t[4];
		Ops::transpose(m, t);
		CHECK(equals<Ops>(t[0], 2, 1, 0, 5) && equals<Ops>(t[3], 0, 0, 0, 1));
		CHECK(std::fabs(simd::determinant<Ops>(m) - 25) < 1e-4);
		reg inv[4], product[4];
		CHECK(simd::inverse<Ops>(m, inv));
		simd::multiply<Ops>(m, inv, product);
		CHECK(equals<Ops>(product[0], 1, 0, 0, 0, 1e-4) && equals<Ops>(product[1], 0, 1, 0, 0, 1e-4) && equals<Ops>(product[2], 0, 0, 1, 0, 1e-4) && equals<Ops>(product[3], 0, 0, 0, 1, 1e-4));
		reg singular[4] = { Ops::set(1, 0, 0, 0), Ops::set(0, 0, 0, 0), Ops::set(0, 0, 1, 0), Ops::set(0, 0, 0, 1) };
		CHECK(!simd::inverse<Ops>(singular, inv));

		//	@tool 行ベクトル規約の形での変換（v * M）
		reg translation[4] = { Ops::set(1, 0, 0, 0), Ops::set(0, 1, 0, 0), Ops::set(0, 0, 1, 0), Ops::set(10, 20, 30, 1) };
		CHECK(equals<Ops>(simd::transform4<Ops>(Ops::set(1, 2, 3, 1), translation), 11, 22, 33, 1));
		CHECK(equals<Ops>(simd::transform3_point<Ops>(Ops::set(1, 2, 3, 0), translation), 11, 22, 33, 1));
		CHECK(equals<Ops>(simd::transform3_normal<Ops>(Ops::set(1, 2, 3, 0), translation), 1, 2, 3, 0));

		//	@tool クォータニオンの積（Z 軸 90 度の後に X 軸 90 度）
		const double h = std::sqrt(0.5);
		const reg qz = Ops::set(0, 0, static_cast<typename Ops::value_type>(h), static_cast<typename Ops::value_type>(h));
		const reg qx = Ops::set(static_cast<typename Ops::value_type>(h), 0, 0, static_cast<typename Ops::value_type>(h));
		//	@tool qz の後に qx = ハミルトン積 qx * qz = (0.5, -0.5, 0.5, 0.5)
		CHECK(equals<Ops>(simd::quaternion_multiply<Ops>(qz, qx), 0.5, -0.5, 0.5, 0.5));

		//	@tool 4次元外積
		CHECK(equals<Ops>(simd::cross4<Ops>(Ops::set(1, 0, 0, 0), Ops::set(0, 1, 0, 0), Ops::set(0, 0, 1, 0)), 0, 0, 0, -1));
	}
}

MATH_TEST(simd_float_ops)
{
	test_ops<simd::f4>();
}

MATH_TEST(simd_double_ops)
{
	test_ops<simd::d4>();
}

//	@tool レイアウト（SIMD 使用時はレジスタ幅で保持し、double の配置は AVX2 の有無で変わらない）
MATH_TEST(simd_layout)
{
	CHECK(sizeof(vector4f) == 16 && sizeof(quaternionf) == 16 && sizeof(matrixf) == 64);
	CHECK(sizeof(vector4d) == 32 && sizeof(quaterniond) == 32 && sizeof(matrixd) == 128);
	if constexpr (use_simd_v<float>)
	{
		CHECK(sizeof(vector2f) == 16 && sizeof(vector3f) == 16 && alignof(vector3f) == 16);
	}
	else
	{
		CHECK(sizeof(vector2f) == 8 && sizeof(vector3f) == 12);
	}
	if constexpr (use_simd_v<double>)
	{
		CHECK(sizeof(vector2d) == 32 && sizeof(vector3d) == 32 && alignof(vector3d) == 32 && alignof(matrixd) == 32);
	}
	else
	{
		CHECK(sizeof(vector2d) == 16 && sizeof(vector3d) == 24);
	}
}
