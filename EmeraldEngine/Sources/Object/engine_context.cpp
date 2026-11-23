#include "Object/engine_context.h"
#include "Object/world.h"

engine_context::engine_context(world& owner)
: owner(owner)
#if defined(USE_IMGUI)
, editor(std::make_unique<editor_context>())
#endif	//	defined(USE_IMGUI)
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
#endif	//	defined(USE_IMGUI)
