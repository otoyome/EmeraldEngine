#pragma once

#include "Math/math_all.h"
#include "cereal/cereal.hpp"

template<typename T>
class transform
{
	using vector3 = math::vector3<T>;
	using quaternion = math::quaternion<T>;
	using matrix = math::matrix<T>;
	using other_value_type = std::conditional_t<std::is_same_v<T, float>, double, float>;

public:
	const vector3& get_scale() const;
	void set_scale(const vector3& scale);
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
	transform();
	transform(const vector3& scale, const quaternion& rotation, const vector3& translation);
	explicit transform(const transform<other_value_type>& v);

	vector3 transform_point(const vector3& point) const;
	vector3 transform_vector(const vector3& vector) const;
	vector3 transform_direction(const vector3& direction) const;

	vector3 inverse_transform_point(const vector3& point) const;
	vector3 inverse_transform_vector(const vector3& vector) const;
	vector3 inverse_transform_direction(const vector3& direction) const;

	//	Œ»İ‚ÌÀ•W‚ÉˆÚ“®—Ê‚ğ‰Á‚¦‚é
	void translate(const vector3& offset);
	//	Œ»İ‚Ì‰ñ“]‚ÌŒã‚É‰ñ“]‚ğ‰Á‚¦‚é
	void rotate(const quaternion& rotation);
	//	Œ»İ‚ÌŠgk—¦‚ÉŠ|‚¯‡‚í‚¹‚é
	void multiply_scale(const vector3& scale);

	//	p¨‚ğ‡¬
	transform multiply(const transform& parent) const;
	//	‹tp¨‚ğæ“¾
	transform inverse() const;
	//	parent ‚©‚çŒ©‚½‘Š‘Îp¨‚ğæ“¾
	transform relative_to(const transform& parent) const;

	//	origin ‚ğŒ´“_‚Æ‚µ‚½ float ‚Ìp¨s—ñ‚ğæ“¾
	math::matrixf to_relative_matrix(const vector3& origin) const;

	//	”äŠr
	bool equal(const transform& v, T tolerance = math::constants<T>::epsilon * static_cast<T>(10)) const;

public:
	transform operator*(const transform& parent) const;
	transform& operator*=(const transform& parent);
	bool operator==(const transform& v) const;
	bool operator!=(const transform& v) const;

	template<class Archive>
	void serialize(Archive& archive, int version);

public:
	//	’PˆÊp¨
	static transform identity();
	//	p¨s—ñ‚©‚ç¶¬
	static transform from_matrix(const matrix& m);
	//	2‚Â‚Ìp¨‚ğ•âŠÔ
	static transform blend(const transform& l, const transform& r, T value);

private:
	vector3 scale;				//	Šgk—¦
	quaternion rotation;		//	‰ñ“]î•ñ
	vector3 translation;		//	À•Wî•ñ
};

using transformf = transform<float>;
using transformd = transform<double>;

extern template class transform<float>;
extern template class transform<double>;

CEREAL_CLASS_VERSION(transformf, 1)
CEREAL_CLASS_VERSION(transformd, 1)
