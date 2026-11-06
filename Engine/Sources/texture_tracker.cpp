#include "texture_tracker.h"

#if defined(USE_IMGUI)
#include <algorithm>

texture_tracker::texture_tracker()
{
}

texture_tracker::~texture_tracker()
{
}

void texture_tracker::add(const texture_ptr& value)
{
	if (!value)
		return;

	std::lock_guard lock(mutex);
	textures.emplace_back(value);
}

std::vector<texture_ptr> texture_tracker::collect()
{
	std::vector<texture_ptr> result;
	std::lock_guard lock(mutex);
	textures.erase(std::remove_if(textures.begin(), textures.end(),
		[&](const std::weak_ptr<texture>& entry)
		{
			auto value = entry.lock();
			if (!value)
				return true;
			result.emplace_back(std::move(value));
			return false;
		}), textures.end());
	return result;
}
#endif	//	defined(USE_IMGUI)
