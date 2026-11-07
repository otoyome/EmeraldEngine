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
