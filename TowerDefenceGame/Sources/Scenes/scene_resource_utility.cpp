#include "Scenes/scene_resource_utility.h"

#include <algorithm>
#include <sstream>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Resource/resource_manager.h"
#include "Object/game_object_serializer.h"
#include "Scenes/scene_system.h"

namespace
{
	std::unique_ptr<scene_system> read_system(cereal::JSONInputArchive& archive)
	{
		std::unique_ptr<scene_system> system;
		try
		{
			archive.setNextName("system");
			archive.startNode();
			std::string type;
			archive(CEREAL_NVP(type));
			if (!type.empty())
			{
				const scene_system_registry* registry = scene_system_registry::instance();
				system = registry ? registry->create(type) : nullptr;
				if (system)
					system->deserialize(archive);
				else
					DEBUG_WARNING_CATEGORY(logger::categorys::system, "scene_resource unknown system type[%s]", type.c_str());
			}
			archive.finishNode();
		}
		catch (const cereal::Exception& exception)
		{
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "scene_resource system load failed[%s]", exception.what());
		}
		return system;
	}
}

bool scene_resource_utility::create(const std::vector<scene_resource_ptr>& scenes, std::vector<created_scene>& out_scenes)
{
	std::vector<created_scene> created(scenes.size());
	for (size_t index = 0; index < scenes.size(); ++index)
	{
		std::unique_ptr<scene_system>& system = created[index].system;
		std::istringstream stream(scenes[index]->get_body());
		if (!game_object_serializer::load(stream, created[index].objects, [&system](cereal::JSONInputArchive& archive) { system = read_system(archive); }))
			return false;
	}
	out_scenes = std::move(created);
	return true;
}

std::string scene_resource_utility::make_display_name(std::string_view path)
{
	const size_t separator = path.find_last_of("/\\");
	std::string_view name = (separator == std::string_view::npos) ? path : path.substr(separator + 1);
	const size_t dot = name.find_last_of('.');
	if (dot != std::string_view::npos && dot > 0)
		name = name.substr(0, dot);
	return std::string(name);
}
