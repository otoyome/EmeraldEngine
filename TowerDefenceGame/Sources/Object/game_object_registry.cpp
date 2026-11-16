#include "Object/game_object_registry.h"

#include <algorithm>

game_object_registry::game_object_registry()
{
	for (auto& entry : get_static_list())
		add(entry.type.name, entry.type.category, entry.function);
}

std::vector<game_object_registry::static_entry>& game_object_registry::get_static_list()
{
	static std::vector<static_entry> list;
	return list;
}

game_object_registry::~game_object_registry()
{
}

void game_object_registry::add(std::string_view type, std::string_view category, create_function function)
{
	assert(!contains(type) && "“¯‚¶Œ^–¼‚ð“ñd‚É“o˜^‚µ‚½");
	functions.insert_or_assign(std::string(type), registered_type{ std::string(category), std::move(function) });
}

game_object_ptr game_object_registry::create(std::string_view type) const
{
	auto it = functions.find(std::string(type));
	if (it == functions.end())
		return nullptr;

	return it->second.function();
}

bool game_object_registry::contains(std::string_view type) const
{
	return functions.find(std::string(type)) != functions.end();
}

std::vector<std::string> game_object_registry::get_type_names() const
{
	std::vector<std::string> names;
	names.reserve(functions.size());
	for (auto& [type, function] : functions)
		names.emplace_back(type);
	std::sort(names.begin(), names.end());
	return names;
}

std::vector<game_object_registry::type_entry> game_object_registry::get_placeable_types() const
{
	std::vector<type_entry> types;
	for (auto& [type, registered] : functions)
	{
		if (!registered.category.empty())
			types.push_back({ type, registered.category });
	}
	std::sort(types.begin(), types.end(), [](const type_entry& a, const type_entry& b)
	{
		if (a.category != b.category)
			return a.category < b.category;
		return a.name < b.name;
	});
	return types;
}
