#include "Object/game_object_registry.h"

game_object_registry::game_object_registry()
{
	for (auto& [type, function] : get_static_list())
		add(type, function);
}

std::vector<std::pair<std::string, game_object_registry::create_function>>& game_object_registry::get_static_list()
{
	static std::vector<std::pair<std::string, create_function>> list;
	return list;
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
