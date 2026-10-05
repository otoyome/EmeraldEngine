#include "CoordinateSystem/transform.h"

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
void transform<T>::set_scale(vector3& scale)
{
	this->scale = scale;
}

template<typename T>
const transform<T>::rotator& transform<T>::get_rotation() const
{
	return rotation;
}

template<typename T>
void transform<T>::set_rotation(rotator& rotation)
{
	this->rotation = rotation;
}

template<typename T>
const transform<T>::vector3& transform<T>::get_translation() const
{
	return translation;
}

template<typename T>
void transform<T>::set_translation(vector3& translation)
{
	this->translation = translation;
}
