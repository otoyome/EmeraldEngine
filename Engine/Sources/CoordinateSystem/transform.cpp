#include "CoordinateSystem/transform.h"

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
const transform<T>::vector3& transform<T>::get_scale() const
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
const transform<T>::quaternion& transform<T>::get_rotation() const
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
const transform<T>::vector3& transform<T>::get_translation() const
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
const transform<T>::matrix& transform<T>::get_transform() const
{
}

template<typename T>
transform<T>::transform()
: system_flags(0)
, scale(1, 1, 1)
, rotation(0.0f, 0.0f, 0.0f)
, translation(0.0f, 0.0f, 0.0f)
{
	set_flag(flags::dirty, true);
}

template<typename T>
void transform<T>::update()
{
	if (!is_flag(flags::dirty))
		return;

	sr = matrix::scaling(scale) * matrix::rotation_quaternion(rotation);
	srt = sr * matrix::translation(translation);
	inverse_srt = matrix::inverse(srt);

	set_flag(flags::dirty, false);
}












