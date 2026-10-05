#include "CoordinateSystem/transform.h"

template<typename T>
transform<T> transform<T>::identity()
{
	return transform();
}

template<typename T>
transform<T> transform<T>::from_matrix(const matrix& m)
{
	vector3 out_scale;
	quaternion out_rotation;
	vector3 out_translation;
	if (!m.decompose(out_scale, out_rotation, out_translation))
	{
		return identity();
	}
	return transform(out_scale, out_rotation, out_translation);
}

template<typename T>
transform<T> transform<T>::blend(const transform& l, const transform& r, T value)
{
	return transform(math::lerp(l.scale, r.scale, value),
					 math::slerp(l.rotation, r.rotation, value),
					 math::lerp(l.translation, r.translation, value));
}

template<typename T>
bool transform<T>::is_flag(flags flag) const
{
	return system_utility::is_bitflag(system_flags, 1 << static_cast<int>(flag));
}

template<typename T>
void transform<T>::set_flag(flags flag, bool value)
{
	system_utility::set_bitflag(system_flags, 1 << static_cast<int>(flag), value);
}

template<typename T>
const typename transform<T>::vector3& transform<T>::get_scale() const
{
	return scale;
}

template<typename T>
void transform<T>::set_scale(const vector3& scale)
{
	this->scale = scale;
	set_flag(flags::dirty, true);
}

template<typename T>
const typename transform<T>::quaternion& transform<T>::get_rotation() const
{
	return rotation;
}

template<typename T>
void transform<T>::set_rotation(const quaternion& rotation)
{
	this->rotation = rotation;
	set_flag(flags::dirty, true);
}

template<typename T>
const typename transform<T>::vector3& transform<T>::get_translation() const
{
	return translation;
}

template<typename T>
void transform<T>::set_translation(const vector3& translation)
{
	this->translation = translation;
	set_flag(flags::dirty, true);
}

template<typename T>
const typename transform<T>::matrix& transform<T>::get_transform() const
{
	update();
	return srt;
}

template<typename T>
const typename transform<T>::matrix& transform<T>::get_inverse_transform() const
{
	update();
	return inverse_srt;
}

template<typename T>
typename transform<T>::vector3 transform<T>::get_forward() const
{
	return rotation.front();
}

template<typename T>
typename transform<T>::vector3 transform<T>::get_right() const
{
	return rotation.right();
}

template<typename T>
typename transform<T>::vector3 transform<T>::get_up() const
{
	return rotation.up();
}

template<typename T>
void transform<T>::translate(const vector3& offset)
{
	set_translation(translation + offset);
}

template<typename T>
void transform<T>::rotate(const quaternion& rotation)
{
	set_rotation(quaternion::concatenate(this->rotation, rotation));
}

template<typename T>
void transform<T>::multiply_scale(const vector3& scale)
{
	set_scale(this->scale * scale);
}

template<typename T>
transform<T>::transform()
: system_flags(0)
, scale(1, 1, 1)
, rotation(quaternion::identity())
, translation(vector3::zero())
{
	set_flag(flags::dirty, true);
}

template<typename T>
transform<T>::transform(const vector3& scale, const quaternion& rotation, const vector3& translation)
: system_flags(0)
, scale(scale)
, rotation(rotation)
, translation(translation)
{
	set_flag(flags::dirty, true);
}

template<typename T>
transform<T>::transform(const transform<other_value_type>& v)
: system_flags(0)
, scale(v.get_scale())
, rotation(v.get_rotation())
, translation(v.get_translation())
{
	set_flag(flags::dirty, true);
}

template<typename T>
typename transform<T>::vector3 transform<T>::transform_point(const vector3& point) const
{
	update();
	return vector3::transform_coord(point, srt);
}

template<typename T>
typename transform<T>::vector3 transform<T>::transform_vector(const vector3& vector) const
{
	update();
	return vector3::transform_normal(vector, srt);
}

template<typename T>
typename transform<T>::vector3 transform<T>::transform_direction(const vector3& direction) const
{
	return rotation.rotate(direction);
}

template<typename T>
typename transform<T>::vector3 transform<T>::inverse_transform_point(const vector3& point) const
{
	return inverse_transform_vector(point - translation);
}

template<typename T>
typename transform<T>::vector3 transform<T>::inverse_transform_vector(const vector3& vector) const
{
	return inverse_transform_direction(vector) / scale;
}

template<typename T>
typename transform<T>::vector3 transform<T>::inverse_transform_direction(const vector3& direction) const
{
	return rotation.inverted().rotate(direction);
}

template<typename T>
transform<T> transform<T>::multiply(const transform& parent) const
{
	return transform(scale * parent.scale,
					 quaternion::concatenate(rotation, parent.rotation),
					 parent.rotation.rotate(translation * parent.scale) + parent.translation);
}

template<typename T>
transform<T> transform<T>::inverse() const
{
	const vector3 inverse_scale = vector3(1, 1, 1) / scale;
	const quaternion inverse_rotation = rotation.inverted();
	return transform(inverse_scale,
					 inverse_rotation,
					 inverse_rotation.rotate(-translation) * inverse_scale);
}

template<typename T>
transform<T> transform<T>::relative_to(const transform& parent) const
{
	const quaternion inverse_parent_rotation = parent.rotation.inverted();
	return transform(scale / parent.scale,
					 quaternion::concatenate(rotation, inverse_parent_rotation),
					 inverse_parent_rotation.rotate(translation - parent.translation) / parent.scale);
}

template<typename T>
math::matrixf transform<T>::to_relative_matrix(const vector3& origin) const
{
	return math::matrixf::transform(math::vector3f(scale),
									math::quaternionf(rotation),
									math::vector3f(translation - origin));
}

template<typename T>
bool transform<T>::equal(const transform& v, T tolerance) const
{
	//	q ‚Æ -q ‚Í“¯‚¶‰ñ“]‚ð•\‚·‚½‚ßA‰ñ“]‚Í“àÏ‚Ìâ‘Î’l‚Å”äŠr‚·‚é
	return math::nearly_equal(scale, v.scale, tolerance)
		&& math::nearly_equal(translation, v.translation, tolerance)
		&& std::abs(rotation.normalized().dot(v.rotation.normalized())) >= static_cast<T>(1) - tolerance;
}

template<typename T>
transform<T> transform<T>::operator*(const transform& parent) const
{
	return multiply(parent);
}

template<typename T>
transform<T>& transform<T>::operator*=(const transform& parent)
{
	*this = multiply(parent);
	return *this;
}

template<typename T>
bool transform<T>::operator==(const transform& v) const
{
	return scale == v.scale && rotation == v.rotation && translation == v.translation;
}

template<typename T>
bool transform<T>::operator!=(const transform& v) const
{
	return !(*this == v);
}

template<typename T>
void transform<T>::update() const
{
	if (!is_flag(flags::dirty))
		return;

	srt = matrix::transform(scale, rotation, translation);
	inverse_srt = matrix::inverse(srt);
	system_utility::set_bitflag(system_flags, 1 << static_cast<int>(flags::dirty), false);
}

template class transform<float>;
template class transform<double>;
