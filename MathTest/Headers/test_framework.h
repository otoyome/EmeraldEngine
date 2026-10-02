#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <vector>

#include "Math/math_all.h"

//	@tool Math ライブラリ用の最小限の単体テストフレームワーク（外部ライブラリに依存しない）
//	@tool MATH_TEST(名前)              ：テストを登録する
//	@tool MATH_TEST_TYPED(名前)        ：float / double の両方でテストを登録する（本体では T が使える）
//	@tool CHECK(式)                     ：式が真であること
//	@tool CHECK_NEAR(実際, 期待, 許容)  ：スカラー / ベクトル / クォータニオン / 行列の各要素の差が許容値以内であること
//	@tool CHECK_SAME_ROTATION(q1, q2, 許容)：符号違いも同じ回転とみなしてクォータニオンを比較する
namespace math_test
{
	struct test_case
	{
		const char* name;
		void (*function)();
	};

	inline std::vector<test_case>& registry()
	{
		static std::vector<test_case> cases;
		return cases;
	}

	struct registrar
	{
		registrar(const char* name, void (*function)())
		{
			registry().push_back({ name, function });
		}
	};

	struct state
	{
		const char* current_test = nullptr;
		std::uint32_t check_count = 0;
		std::uint32_t failure_count = 0;
		bool current_failed = false;
	};

	inline state& current_state()
	{
		static state s;
		return s;
	}

	inline void report(bool ok, const char* expression, const char* file, int line, double error = 0)
	{
		state& s = current_state();
		++s.check_count;
		if (ok)
		{
			return;
		}
		++s.failure_count;
		s.current_failed = true;
		const char* file_name = std::strrchr(file, '\\');
		file_name = file_name ? file_name + 1 : file;
		std::printf("  FAIL [%s] %s(%d): %s (差 = %g)\n", s.current_test, file_name, line, expression, error);
	}

	//	@tool 型ごとの標準の許容値
	template<typename T>
	constexpr double tolerance() { return std::is_same_v<T, float> ? 2e-4 : 1e-9; }

	//	@tool 要素ごとの差の最大値（スカラーと element_count / operator[] を持つ型に対応）
	template<typename A>
	double max_difference(const A& a, const A& b)
	{
		if constexpr (std::is_arithmetic_v<A>)
		{
			return std::fabs(static_cast<double>(a) - static_cast<double>(b));
		}
		else
		{
			double difference = 0;
			for (std::uint32_t i = 0; i < A::element_count; ++i)
			{
				double d = std::fabs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
				difference = d > difference || std::isnan(d) ? d : difference;
			}
			return difference;
		}
	}

	template<typename A, typename B>
	void check_near(const A& actual, const B& expected, double allowed, const char* expression, const char* file, int line)
	{
		const double difference = max_difference<A>(actual, static_cast<A>(expected));
		report(difference <= allowed, expression, file, line, difference);
	}

	template<typename T>
	void check_same_rotation(const math::quaternion<T>& a, const math::quaternion<T>& b, double allowed, const char* expression, const char* file, int line)
	{
		const double difference = std::fmin(max_difference(a, b), max_difference(a, -b));
		report(difference <= allowed, expression, file, line, difference);
	}

	//	@tool 全テストを実行し、失敗数を返す（filter を指定すると名前に filter を含むテストだけ実行する）
	inline int run_all(const char* filter)
	{
		state& s = current_state();
		std::uint32_t failed_tests = 0;
		std::uint32_t executed_tests = 0;
		for (const test_case& test : registry())
		{
			if (filter && !std::strstr(test.name, filter))
			{
				continue;
			}
			s.current_test = test.name;
			s.current_failed = false;
			test.function();
			++executed_tests;
			if (s.current_failed)
			{
				++failed_tests;
			}
		}
		std::printf("テスト %u 件（チェック %u 件）：失敗 %u 件（チェック失敗 %u 件）\n", executed_tests, s.check_count, failed_tests, s.failure_count);
		return static_cast<int>(failed_tests);
	}

}	//	namespace math_test

#define	MATH_TEST_CONCAT_INNER(a, b)	a##b
#define	MATH_TEST_CONCAT(a, b)			MATH_TEST_CONCAT_INNER(a, b)

#define	MATH_TEST(name)																										\
	static void name();																										\
	static ::math_test::registrar MATH_TEST_CONCAT(name, _registrar)(#name, &name);											\
	static void name()

#define	MATH_TEST_TYPED(name)																								\
	template<typename T> static void name();																				\
	static ::math_test::registrar MATH_TEST_CONCAT(name, _registrar_float)(#name "<float>", &name<float>);					\
	static ::math_test::registrar MATH_TEST_CONCAT(name, _registrar_double)(#name "<double>", &name<double>);				\
	template<typename T> static void name()

#define	CHECK(expression)							::math_test::report(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
#define	CHECK_NEAR(actual, expected, allowed)		::math_test::check_near((actual), (expected), (allowed), #actual " ~= " #expected, __FILE__, __LINE__)
#define	CHECK_SAME_ROTATION(actual, expected, allowed)	::math_test::check_same_rotation((actual), (expected), (allowed), #actual " ~= " #expected, __FILE__, __LINE__)
