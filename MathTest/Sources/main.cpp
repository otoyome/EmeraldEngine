#include "test_framework.h"

//	@tool Math ライブラリの単体テスト
//	@tool 使い方：MathTest.exe [テスト名の一部]（省略時は全テスト）
//	@tool 規約（行 / 列ベクトル、左手 / 右手系、深度の向き、SIMD）の全組み合わせは run_math_tests.bat で確認する
int main(int argc, char** argv)
{
	std::printf("Math ライブラリ単体テスト：%s / %s / %s / SIMD(float=%d, double=%d%s)\n",
		MATH_COLUMN_VECTOR ? "列ベクトル" : "行ベクトル",
		MATH_RIGHT_HANDED ? "右手系" : "左手系",
		MATH_REVERSE_DEPTH ? "リバースデプス" : "通常深度",
		MATH_USE_SIMD_FLOAT, MATH_USE_SIMD_DOUBLE,
#if defined(__AVX2__)
		", AVX2"
#else
		""
#endif
	);

	const char* filter = argc > 1 ? argv[1] : nullptr;
	return math_test::run_all(filter) == 0 ? 0 : 1;
}
