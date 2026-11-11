#include "Object/game_object_registry.h"

#include "Object/spatial_object.h"
#include "Object/light_object.h"
#include "Object/model_object.h"
#include "Object/GameObjects/stage_object.h"
#include "Object/GameObjects/System/start_point_object.h"

game_object_registry::game_object_registry()
{
	add<game_object>();
	add<spatial_object>();
	add<directional_light_object>();
	add<point_light_object>();
	add<spot_light_object>();
	add<sky_light_object>();
	add<area_light_probe_object>();
	add<model_object>();
	add<stage_object>();
	add<start_point_object>();
}

game_object_registry::~game_object_registry()
{
}

void game_object_registry::add(std::string_view type, create_function function)
{
	assert(!contains(type) && "“¯‚¶Œ^–¼‚ð“ñd‚É“o˜^‚µ‚½");
	functions.insert_or_assign(std::string(type), std::move(function));
}

game_object_ptr game_object_registry::create(std::string_view type) const
{
	auto it = functions.find(std::string(type));
	if (it == functions.end())
		return nullptr;

	return it->second();
}

bool game_object_registry::contains(std::string_view type) const
{
	return functions.find(std::string(type)) != functions.end();
}
