#pragma once

#include "Math/math_common.h"

namespace math
{
	//	オイラー角で操作しクォータニオンを算出するヘルパークラス
	//	@tool 既存の math::rotator（System/math_utility.h）と名前が衝突するため basic_rotator としている
	template<typename T>
	class basic_rotator
	{
		static_assert(is_supported_scalar_v<T>, "basic_rotator は float / double のみ対応");

	public:
		using value_type = T;
		static constexpr std::uint32_t element_count = 3;	//	要素数

	public:
		constexpr basic_rotator() : x(0), y(0), z(0) {}
		constexpr basic_rotator(T pitch, T yaw, T roll) : x(pitch), y(yaw), z(roll) {}
		explicit basic_rotator(const vector3<T>& euler);
		basic_rotator(const quaternion<T>& rotation);
		//	@tool float <> double の変換（精度が変わるため明示的に行う）
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr basic_rotator(const basic_rotator<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}

		//	前方向
		[[nodiscard]] vector3<T> front() const;

		//	上方向
		[[nodiscard]] vector3<T> up() const;

		//	右方向
		[[nodiscard]] vector3<T> right() const;

		//	@tool オイラー角 / クォータニオンとして取得
		[[nodiscard]] vector3<T> to_vector3() const;
		[[nodiscard]] quaternion<T> to_quaternion() const;

	public:
		operator quaternion<T>() const;
		basic_rotator operator*(const basic_rotator& r) const;
		basic_rotator& operator*=(const basic_rotator& r);
		basic_rotator operator*(const quaternion<T>& r) const;
		basic_rotator& operator*=(const quaternion<T>& r);
		bool operator==(const basic_rotator& r) const { return x == r.x && y == r.y && z == r.z; }
		bool operator!=(const basic_rotator& r) const { return !(*this == r); }

	public:
		union
		{
			T elements[element_count];
			struct { T pitch, yaw, roll; };
			struct { T x, y, z; };
		};
	};

	using rotatorf = basic_rotator<float>;
	using rotatord = basic_rotator<double>;

	extern template class basic_rotator<float>;
	extern template class basic_rotator<double>;

}	//	namespace math
