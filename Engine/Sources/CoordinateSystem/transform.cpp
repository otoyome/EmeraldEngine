#include "CoordinateSystem/transform.h"

template<typename T>
bool transform<T>::is_flag(flags flag) const
{
	return system_utility::is_bitflag(1 << static_cast<int>(flag));
}

template<typename T>
void transform<T>::set_flag(flags flag, bool value)
{
	system_utility::set_bitflag(1 << static_cast<int>(flag), value);
}
