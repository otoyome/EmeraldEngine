#include "Object/System/locator_object.h"
#include "Object/game_object_registry.h"
#include "Scenes/scene_instance.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

std::string_view locator_object::get_tag() const
{
	return tag;
}

void locator_object::set_tag(std::string_view tag)
{
	this->tag = tag;
}

void locator_object::collect(const scene_instance& scene, std::string_view tag, std::vector<std::shared_ptr<locator_object>>& out)
{
	for (const std::weak_ptr<game_object>& weak_object : scene.get_objects())
	{
		game_object_ptr object = weak_object.lock();
		if (!object || !object->check_type(type_name()))
			continue;

		std::shared_ptr<locator_object> locator = std::static_pointer_cast<locator_object>(object);
		if (tag.empty() || locator->get_tag() == tag)
			out.emplace_back(std::move(locator));
	}
}

locator_object::locator_object(std::string_view name)
: spatial_object(name)
{
}

locator_object::locator_object(std::string_view name, std::string_view tag)
: spatial_object(name)
, tag(tag)
{
}

locator_object::~locator_object()
{
}

void locator_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	archive(CEREAL_NVP(tag));
}

void locator_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	archive(CEREAL_NVP(tag));
}

#if defined(USE_IMGUI)
void locator_object::debug_update(float elapsed_time)
{
}

void locator_object::imgui_render_core()
{
	base::imgui_render_core();

	ImGui::InputTextUtf8("tag", tag);
}
#endif	//	defined(USE_IMGUI)

