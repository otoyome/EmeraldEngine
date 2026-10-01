#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

//	@tool 保存・受け渡し用の値型（Framework の ElementValue.h 相当）
//	@tool 演算用の vector2 / vector3 / vector4 / quaternion / matrix は SIMD レジスタ幅で保持するため、
//	@tool 配列での大量保持・GPU への転送・シリアライズなどにはこちらの詰めた型を使い、必要な時だけ相互に変換する
//	@tool 要素は詰めて並ぶ（float3 は 12 バイト、float4x4 は 64 バイト）
namespace math
{
	//	2要素
	template<typename T>
	struct basic_element2
	{
		using value_type = T;
		static constexpr std::uint32_t element_count = 2;	//	要素数

		union
		{
			T elements[element_count];
			struct { T x, y; };
		};

		constexpr basic_element2() : x(0), y(0) {}
		constexpr basic_element2(T x, T y) : x(x), y(y) {}
		//	@tool 要素の型が違うものからの変換（精度が変わるため明示的に行う）
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr basic_element2(const basic_element2<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}

		T& operator[](std::uint32_t index) { return elements[index]; }
		constexpr const T& operator[](std::uint32_t index) const { return elements[index]; }
		constexpr bool operator==(const basic_element2& v) const { return x == v.x && y == v.y; }
		constexpr bool operator!=(const basic_element2& v) const { return !(*this == v); }
	};

	//	3要素
	template<typename T>
	struct basic_element3
	{
		using value_type = T;
		static constexpr std::uint32_t element_count = 3;	//	要素数

		union
		{
			T elements[element_count];
			struct { T x, y, z; };
		};

		constexpr basic_element3() : x(0), y(0), z(0) {}
		constexpr basic_element3(T x, T y, T z) : x(x), y(y), z(z) {}
		//	@tool 要素の型が違うものからの変換（精度が変わるため明示的に行う）
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr basic_element3(const basic_element3<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}

		T& operator[](std::uint32_t index) { return elements[index]; }
		constexpr const T& operator[](std::uint32_t index) const { return elements[index]; }
		constexpr bool operator==(const basic_element3& v) const { return x == v.x && y == v.y && z == v.z; }
		constexpr bool operator!=(const basic_element3& v) const { return !(*this == v); }
	};

	//	4要素
	template<typename T>
	struct basic_element4
	{
		using value_type = T;
		static constexpr std::uint32_t element_count = 4;	//	要素数

		union
		{
			T elements[element_count];
			struct { T x, y, z, w; };
		};

		constexpr basic_element4() : x(0), y(0), z(0), w(0) {}
		constexpr basic_element4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
		//	@tool 要素の型が違うものからの変換（精度が変わるため明示的に行う）
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr basic_element4(const basic_element4<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)), w(static_cast<T>(v.w)) {}

		T& operator[](std::uint32_t index) { return elements[index]; }
		constexpr const T& operator[](std::uint32_t index) const { return elements[index]; }
		constexpr bool operator==(const basic_element4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
		constexpr bool operator!=(const basic_element4& v) const { return !(*this == v); }
	};

	//	行列（行優先で Rows 行 Columns 列）
	//	@tool 既定値は対角成分が 1 の行列（4x4 なら単位行列）
	template<typename T, std::uint32_t Rows, std::uint32_t Columns>
	struct basic_element_matrix
	{
		using value_type = T;
		static constexpr std::uint32_t row_count = Rows;			//	行数
		static constexpr std::uint32_t column_count = Columns;		//	列数
		static constexpr std::uint32_t element_count = Rows * Columns;	//	要素数

		union
		{
			T elements[element_count];
			T m[Rows][Columns];
		};

		constexpr basic_element_matrix() : elements{}
		{
			for (std::uint32_t i = 0; i < (Rows < Columns ? Rows : Columns); ++i)
			{
				elements[i * Columns + i] = static_cast<T>(1);
			}
		}
		//	@tool 要素の型が違うものからの変換（精度が変わるため明示的に行う）
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr basic_element_matrix(const basic_element_matrix<U, Rows, Columns>& v) : elements{}
		{
			for (std::uint32_t i = 0; i < element_count; ++i)
			{
				elements[i] = static_cast<T>(v.elements[i]);
			}
		}

		T& operator()(std::uint32_t row, std::uint32_t column) { return m[row][column]; }
		constexpr const T& operator()(std::uint32_t row, std::uint32_t column) const { return m[row][column]; }
		constexpr bool operator==(const basic_element_matrix& v) const
		{
			for (std::uint32_t i = 0; i < element_count; ++i)
			{
				if (elements[i] != v.elements[i])
				{
					return false;
				}
			}
			return true;
		}
		constexpr bool operator!=(const basic_element_matrix& v) const { return !(*this == v); }
	};

	//	@tool 別名（HLSL の型名に合わせている）
	using float2 = basic_element2<float>;
	using float3 = basic_element3<float>;
	using float4 = basic_element4<float>;
	using double2 = basic_element2<double>;
	using double3 = basic_element3<double>;
	using double4 = basic_element4<double>;
	using int2 = basic_element2<std::int32_t>;
	using int3 = basic_element3<std::int32_t>;
	using int4 = basic_element4<std::int32_t>;
	using uint2 = basic_element2<std::uint32_t>;
	using uint3 = basic_element3<std::uint32_t>;
	using uint4 = basic_element4<std::uint32_t>;
	using short2 = basic_element2<std::int16_t>;
	using short3 = basic_element3<std::int16_t>;
	using short4 = basic_element4<std::int16_t>;
	using ushort2 = basic_element2<std::uint16_t>;
	using ushort3 = basic_element3<std::uint16_t>;
	using ushort4 = basic_element4<std::uint16_t>;

	using float2x2 = basic_element_matrix<float, 2, 2>;
	using float3x3 = basic_element_matrix<float, 3, 3>;
	using float3x4 = basic_element_matrix<float, 3, 4>;
	using float4x3 = basic_element_matrix<float, 4, 3>;
	using float4x4 = basic_element_matrix<float, 4, 4>;
	using double3x3 = basic_element_matrix<double, 3, 3>;
	using double3x4 = basic_element_matrix<double, 3, 4>;
	using double4x3 = basic_element_matrix<double, 4, 3>;
	using double4x4 = basic_element_matrix<double, 4, 4>;

	static_assert(sizeof(float3) == 12 && sizeof(float4x4) == 64 && sizeof(float4x3) == 48, "要素は詰めて並ぶ必要がある");

}	//	namespace math
