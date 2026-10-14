#include "object/gameobjects/system/start_point_object.h"

std::string_view start_point_object::get_tag() const
{
	return tag;
}

void start_point_object::set_tag(std::string_view tag)
{
	this->tag = tag;
}

start_point_object::start_point_object(std::string_view name, std::string_view tag)
: spatial_object(name)
, tag(tag)
{
}

start_point_object::~start_point_object()
{
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
