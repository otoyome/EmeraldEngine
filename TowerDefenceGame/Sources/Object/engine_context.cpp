#include "Object/engine_context.h"
#include "Object/world.h"

engine_context::engine_context(world& owner)
: owner(owner)
{
}

object_manager& engine_context::get_objects() const
{
	return owner.get_objects();
}

scene_manager& engine_context::get_scenes() const
{
	return owner.get_scenes();
}

effect_manager& engine_context::get_effects() const
{
	return owner.get_effects();
}

#if defined(USE_IMGUI)
debug_primitive_collector& engine_context::get_debug_primitives() const
{
	return owner.get_debug_primitives();
}

bool engine_context::begin_window(std::string_view name, bool* open, int flags)
{
	if (embedded && dock_id != 0)
		ImGui::SetNextWindowDockID(dock_id, ImGuiCond_Once);	//	起動して最初に開いた時は必ずドッキングする
	return ImGui::Begin(make_window_name(name).c_str(), open, flags);
}
#endif	//	defined(USE_IMGUI)
