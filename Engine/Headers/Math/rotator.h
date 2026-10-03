#pragma once

#include "Math/math_common.h"

namespace math
{
	//	オイラー角で操作しクォータニオンを算出するヘルパークラス
	template<typename T>
	class basic_rotator
	{
		static_assert(is_supported_scalar_v<T>, "basic_rotator は float / double のみ対応");

	public:
		using value_type = T;
		static constexpr std::uint32_t element_count = 3;	//	要素数

	public:
		static constexpr basic_rotator zero() { return basic_rotator(); }

	public:
		constexpr basic_rotator() : x(0), y(0), z(0) {}
		constexpr basic_rotator(T pitch, T yaw, T roll) : x(pitch), y(yaw), z(roll) {}
		explicit basic_rotator(const vector3<T>& euler);
		explicit basic_rotator(const quaternion<T>& rotation, rotation_order order = default_rotation_order);
		explicit basic_rotator(const matrix<T>& rotation, rotation_order order = default_rotation_order);
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit constexpr basic_rotator(const basic_rotator<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}

		//	前方向
		[[nodiscard]] vector3<T> front() const;

		//	上方向
		[[nodiscard]] vector3<T> up() const;

		//	右方向
		[[nodiscard]] vector3<T> right() const;

		//	オイラー角 / クォータニオンとして取得
		[[nodiscard]] vector3<T> to_vector3() const;
		[[nodiscard]] quaternion<T> to_quaternion(rotation_order order = default_rotation_order) const;

		//	ベクトルを回転させる / 逆回転させる
		[[nodiscard]] vector3<T> rotate(const vector3<T>& v) const;
		[[nodiscard]] vector3<T> unrotate(const vector3<T>& v) const;

		//	逆回転を表す rotator（クォータニオンを経由するため、角度は -π～π に折り返される）
		[[nodiscard]] basic_rotator inverse() const;

		//	各軸の角度を -π～π に折り返す
		void normalize();
		[[nodiscard]] basic_rotator normalized() const { basic_rotator ret = *this; ret.normalize(); return ret; }

		//	各軸の角度を min_angle ～ max_angle に制限
		void clamp(const basic_rotator& min_angle, const basic_rotator& max_angle);
		[[nodiscard]] basic_rotator clamped(const basic_rotator& min_angle, const basic_rotator& max_angle) const { basic_rotator ret = *this; ret.clamp(min_angle, max_angle); return ret; }

		//	各軸の角度の差が tolerance 以下か（2π ずれた角度は同じとみなす）
		[[nodiscard]] bool nearly_equal(const basic_rotator& r, T tolerance = static_cast<T>(1.0e-4)) const;
		[[nodiscard]] bool nearly_zero(T tolerance = static_cast<T>(1.0e-4)) const { return nearly_equal(zero(), tolerance); }

		//	各軸ごとに最短経路で補間
		[[nodiscard]] static basic_rotator lerp(const basic_rotator& l, const basic_rotator& r, T value);

		//	front() が direction を向く rotator を生成
		[[nodiscard]] static basic_rotator from_direction(const vector3<T>& direction);

	public:
		operator quaternion<T>() const;
		basic_rotator operator+(const basic_rotator& r) const { return basic_rotator(x + r.x, y + r.y, z + r.z); }
		basic_rotator operator-(const basic_rotator& r) const { return basic_rotator(x - r.x, y - r.y, z - r.z); }
		basic_rotator operator-() const { return basic_rotator(-x, -y, -z); }
		basic_rotator operator*(T value) const { return basic_rotator(x * value, y * value, z * value); }
		basic_rotator operator/(T value) const { return *this * (static_cast<T>(1) / value); }
		friend basic_rotator operator*(T value, const basic_rotator& r) { return r * value; }
		basic_rotator& operator+=(const basic_rotator& r) { return *this = *this + r; }
		basic_rotator& operator-=(const basic_rotator& r) { return *this = *this - r; }
		basic_rotator& operator*=(T value) { return *this = *this * value; }
		basic_rotator& operator/=(T value) { return *this = *this / value; }
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
