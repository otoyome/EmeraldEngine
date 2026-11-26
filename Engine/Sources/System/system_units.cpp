#include "System/system_units.h"

#include <cmath>

#include "Math/utility.h"

float system_units::convert_rate(float rate, float elapsed_time)
{
	return 1.0f - std::pow(1.0f - math::saturate(rate), elapsed_time / convert_fps_to_sec(1.0f));
}
