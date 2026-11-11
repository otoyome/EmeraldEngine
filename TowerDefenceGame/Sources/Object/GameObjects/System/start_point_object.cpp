#include "object/gameobjects/system/start_point_object.h"
#include "Object/game_object_registry.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

std::string_view start_point_object::get_tag() const
{
	return tag;
}

void start_point_object::set_tag(std::string_view tag)
{
	this->tag = tag;
}

start_point_object::start_point_object(std::string_view name)
: spatial_object(name)
{
}

start_point_object::start_point_object(std::string_view name, std::string_view tag)
: spatial_object(name)
, tag(tag)
{
}

start_point_object::~start_point_object()
{
}

void start_point_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	archive(CEREAL_NVP(tag));
}

void start_point_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	archive(CEREAL_NVP(tag));
}

#if defined(USE_IMGUI)
void start_point_object::debug_update(float elapsed_time)
{
}

void start_point_object::imgui_render_core()
{
	base::imgui_render_core();

	ImGui::InputTextUtf8("tag", tag);
}
#endif	//	defined(USE_IMGUI)

GAME_OBJECT_REGISTER(start_point_object);
