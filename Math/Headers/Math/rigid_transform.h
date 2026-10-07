#pragma once

#include "Math/math_all.h"
#include "cereal/cereal.hpp"

namespace math
{
	template<typename T>
	class rigid_transform
	{
		using vector3 = math::vector3<T>;
		using quaternion = math::quaternion<T>;
		using matrix = math::matrix<T>;
		using other_value_type = std::conditional_t<std::is_same_v<T, float>, double, float>;

	public:
		const quaternion& get_rotation() const;
		void set_rotation(const quaternion& rotation);
		const vector3& get_translation() const;
		void set_translation(const vector3& translation);

		matrix get_transform() const;
		matrix get_inverse_transform() const;

		vector3 get_right() const;
		vector3 get_up() const;
		vector3 get_front() const;

		math::rotator<T> get_euler() const;
		void set_euler(const math::rotator<T>& euler);

	public:
		rigid_transform();
		rigid_transform(const quaternion& rotation, const vector3& translation);
		explicit rigid_transform(const rigid_transform<other_value_type>& v);

		vector3 transform_point(const vector3& point) const;
		vector3 transform_direction(const vector3& direction) const;

		vector3 inverse_transform_point(const vector3& point) const;
		vector3 inverse_transform_direction(const vector3& direction) const;

		//	現在の座標に移動量を加える
		void translate(const vector3& offset);
		//	現在の回転の後に回転を加える
		void rotate(const quaternion& rotation);

		//	姿勢を合成（行ベクトル規約に合わせ、*this の後に parent を適用する）
		rigid_transform multiply(const rigid_transform& parent) const;
		//	逆姿勢を取得
		rigid_transform inverse() const;
		//	parent から見た相対姿勢を取得（multiply の逆操作）
		rigid_transform relative_to(const rigid_transform& parent) const;

		//	origin を原点とした float の姿勢行列を取得
		math::matrixf to_relative_matrix(const vector3& origin) const;

		//	誤差を許容して比較（q と -q は同じ回転として扱う）
		bool equal(const rigid_transform& v, T tolerance = math::constants<T>::epsilon * static_cast<T>(10)) const;

	public:
		rigid_transform operator*(const rigid_transform& parent) const;
		rigid_transform& operator*=(const rigid_transform& parent);
		bool operator==(const rigid_transform& v) const;
		bool operator!=(const rigid_transform& v) const;

		template<class Archive>
		void serialize(Archive& archive, int version);

	public:
		//	単位姿勢
		static rigid_transform identity();
		//	姿勢行列から生成
		static rigid_transform from_matrix(const matrix& m);
		//	2つの姿勢を補間
		static rigid_transform blend(const rigid_transform& l, const rigid_transform& r, T value);

	private:
		quaternion rotation;		//	回転情報
		vector3 translation;		//	座標情報
	};

	using rigid_transformf = rigid_transform<float>;
	using rigid_transformd = rigid_transform<double>;

	extern template class rigid_transform<float>;
	extern template class rigid_transform<double>;

}	//	namespace math

//	cereal のバージョン（呼び出し側で参照されるためヘッダーで定義する）
CEREAL_CLASS_VERSION(math::rigid_transformf, 1)
CEREAL_CLASS_VERSION(math::rigid_transformd, 1)
