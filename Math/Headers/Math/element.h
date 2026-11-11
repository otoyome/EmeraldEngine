#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

//	ï€ë∂ÅEéÛÇØìnÇµópÇÃílå^
namespace math
{
	//	2óvëf
	template<typename T>
	struct element2
	{
		using value_type = T;
		static constexpr std::uint32_t element_count = 2;	//	óvëfêî

		union
		{
			T elements[element_count];
			struct { T x, y; };
		};

		element2() = default;
		constexpr element2(T x, T y) : x(x), y(y) {}

		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr element2(const element2<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}

		T& operator[](std::uint32_t index) { return elements[index]; }
		constexpr const T& operator[](std::uint32_t index) const { return elements[index]; }
		constexpr bool operator==(const element2& v) const { return x == v.x && y == v.y; }
		constexpr bool operator!=(const element2& v) const { return !(*this == v); }
	};

	//	3óvëf
	template<typename T>
	struct element3
	{
		using value_type = T;
		static constexpr std::uint32_t element_count = 3;	//	óvëfêî

		union
		{
			T elements[element_count];
			struct { T x, y, z; };
		};

		element3() = default;
		constexpr element3(T x, T y, T z) : x(x), y(y), z(z) {}

		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr element3(const element3<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}

		T& operator[](std::uint32_t index) { return elements[index]; }
		constexpr const T& operator[](std::uint32_t index) const { return elements[index]; }
		constexpr bool operator==(const element3& v) const { return x == v.x && y == v.y && z == v.z; }
		constexpr bool operator!=(const element3& v) const { return !(*this == v); }
	};

	//	4óvëf
	template<typename T>
	struct element4
	{
		using value_type = T;
		static constexpr std::uint32_t element_count = 4;	//	óvëfêî

		union
		{
			T elements[element_count];
			struct { T x, y, z, w; };
		};

		element4() = default;
		constexpr element4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}

		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr element4(const element4<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)), w(static_cast<T>(v.w)) {}

		T& operator[](std::uint32_t index) { return elements[index]; }
		constexpr const T& operator[](std::uint32_t index) const { return elements[index]; }
		constexpr bool operator==(const element4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
		constexpr bool operator!=(const element4& v) const { return !(*this == v); }
	};

	//	çsóÒ
	template<typename T, std::uint32_t Rows, std::uint32_t Columns>
	struct element_matrix
	{
		using value_type = T;
		static constexpr std::uint32_t row_count = Rows;			//	çsêî
		static constexpr std::uint32_t column_count = Columns;		//	óÒêî
		static constexpr std::uint32_t element_count = Rows * Columns;	//	óvëfêî

		union
		{
			T elements[element_count];
			T m[Rows][Columns];
		};

		constexpr element_matrix() : elements{}
		{
			for (std::uint32_t i = 0; i < (Rows < Columns ? Rows : Columns); ++i)
			{
				elements[i * Columns + i] = static_cast<T>(1);
			}
		}

		template<typename... Args, std::enable_if_t<sizeof...(Args) == element_count && (std::is_arithmetic_v<Args> && ...), int> = 0>
		constexpr element_matrix(Args... values) : elements{ static_cast<T>(values)... } {}

		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr element_matrix(const element_matrix<U, Rows, Columns>& v) : elements{}
		{
			for (std::uint32_t i = 0; i < element_count; ++i)
			{
				elements[i] = static_cast<T>(v.elements[i]);
			}
		}

		T& operator()(std::uint32_t row, std::uint32_t column) { return m[row][column]; }
		constexpr const T& operator()(std::uint32_t row, std::uint32_t column) const { return m[row][column]; }
		constexpr bool operator==(const element_matrix& v) const
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
		constexpr bool operator!=(const element_matrix& v) const { return !(*this == v); }
	};

	template<typename T>
	struct element_matrix<T, 4, 4>
	{
		using value_type = T;
		static constexpr std::uint32_t row_count = 4;		//	çsêî
		static constexpr std::uint32_t column_count = 4;	//	óÒêî
		static constexpr std::uint32_t element_count = 16;	//	óvëfêî

		union
		{
			T elements[element_count];
			T m[4][4];
			struct
			{
				T _11, _12, _13, _14;
				T _21, _22, _23, _24;
				T _31, _32, _33, _34;
				T _41, _42, _43, _44;
			};
		};

		constexpr element_matrix() : elements{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } {}

		template<typename... Args, std::enable_if_t<sizeof...(Args) == element_count && (std::is_arithmetic_v<Args> && ...), int> = 0>
		constexpr element_matrix(Args... values) : elements{ static_cast<T>(values)... } {}

		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr element_matrix(const element_matrix<U, 4, 4>& v) : elements{}
		{
			for (std::uint32_t i = 0; i < element_count; ++i)
			{
				elements[i] = static_cast<T>(v.elements[i]);
			}
		}

		T& operator()(std::uint32_t row, std::uint32_t column) { return m[row][column]; }
		constexpr const T& operator()(std::uint32_t row, std::uint32_t column) const { return m[row][column]; }
		constexpr bool operator==(const element_matrix& v) const
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
		constexpr bool operator!=(const element_matrix& v) const { return !(*this == v); }
	};

	using float2 = element2<float>;
	using float3 = element3<float>;
	using float4 = element4<float>;
	using double2 = element2<double>;
	using double3 = element3<double>;
	using double4 = element4<double>;
	using int2 = element2<std::int32_t>;
	using int3 = element3<std::int32_t>;
	using int4 = element4<std::int32_t>;
	using uint = std::uint32_t;
	using uint2 = element2<std::uint32_t>;
	using uint3 = element3<std::uint32_t>;
	using uint4 = element4<std::uint32_t>;
	using short2 = element2<std::int16_t>;
	using short3 = element3<std::int16_t>;
	using short4 = element4<std::int16_t>;
	using ushort = std::uint16_t;
	using ushort2 = element2<std::uint16_t>;
	using ushort3 = element3<std::uint16_t>;
	using ushort4 = element4<std::uint16_t>;

	using float2x2 = element_matrix<float, 2, 2>;
	using float3x3 = element_matrix<float, 3, 3>;
	using float3x4 = element_matrix<float, 3, 4>;
	using float4x3 = element_matrix<float, 4, 3>;
	using float4x4 = element_matrix<float, 4, 4>;
	using double3x3 = element_matrix<double, 3, 3>;
	using double3x4 = element_matrix<double, 3, 4>;
	using double4x3 = element_matrix<double, 4, 3>;
	using double4x4 = element_matrix<double, 4, 4>;

}	//	namespace math
