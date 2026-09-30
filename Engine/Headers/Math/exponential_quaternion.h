#pragma once

#include "Math/math_common.h"

namespace math
{
	//	対数クォータニオン演算クラス
	//	最低限の実装しかしていない
	//	@tool 3要素のみのため float / double ともスカラー処理
	template<typename T>
	class exponential_quaternion
	{
		static_assert(is_supported_scalar_v<T>, "exponential_quaternion は float / double のみ対応");

	public:
		using value_type = T;
		static constexpr std::uint32_t element_count = 3;	//	要素数

	public:
		constexpr exponential_quaternion() : x(0), y(0), z(0) {}
		constexpr exponential_quaternion(T x, T y, T z) : x(x), y(y), z(z) {}
		explicit exponential_quaternion(const quaternion<T>& q);
		//	@tool float <> double の変換（精度が変わるため明示的に行う）
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr exponential_quaternion(const exponential_quaternion<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}

		//	@tool クォータニオンに変換
		quaternion<T> to_quaternion() const;
		explicit operator quaternion<T>() const;

	public:
		T& operator[](std::int32_t index) { return elements[index]; }
		const T& operator[](std::int32_t index) const { return elements[index]; }
		exponential_quaternion operator+(const exponential_quaternion& v) const { return exponential_quaternion(x + v.x, y + v.y, z + v.z); }
		exponential_quaternion& operator+=(const exponential_quaternion& v) { x += v.x; y += v.y; z += v.z; return *this; }
		exponential_quaternion operator*(T value) const { return exponential_quaternion(x * value, y * value, z * value); }
		exponential_quaternion& operator*=(T value) { x *= value; y *= value; z *= value; return *this; }

	public:
		union
		{
			T elements[element_count];
			struct { T x, y, z; };
		};
	};

	using exponential_quaternionf = exponential_quaternion<float>;
	using exponential_quaterniond = exponential_quaternion<double>;

	extern template class exponential_quaternion<float>;
	extern template class exponential_quaternion<double>;

}	//	namespace math
