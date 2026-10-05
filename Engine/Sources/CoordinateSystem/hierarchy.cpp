#include "CoordinateSystem/hierarchy.h"

hierarchy::hierarchy()
: system_flags(0)
{
	set_flag(flags::dirty, true);
}

bool hierarchy::is_flag(flags flag) const
{
	return system_utility::is_bitflag(1 << static_cast<int>(flag));
}

void hierarchy::set_flag(flags flag, bool value)
{
	system_utility::set_bitflag(1 << static_cast<int>(flag), value);
}
