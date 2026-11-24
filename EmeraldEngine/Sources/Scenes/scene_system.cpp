#include "Scenes/scene_system.h"

#include <algorithm>

#include "Scenes/scene_instance.h"
#include "Scenes/scene_manager.h"
#include "Object/engine_context.h"

scene_instance& scene_system::get_scene() const
{
	assert(owner && "シーンに登録されていないシーンシステム");
	return *owner;
}

engine_context& scene_system::get_context() const
{
	assert(context && "ワールドに登録されていないシーンシステム");
	return *context;
}

#if defined(USE_IMGUI)
void scene_system::imgui_render_window(std::string_view scene_path)
{
	if (!window_opened)
		return;

	const core::cache_string path(scene_path);
	const std::string title = std::string(get_type_name()) + " - " + std::string(path.get<core::utf8_cache>()) + (is_editing() ? " (editing)" : "")
		+ "###scene_system" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
	if (ImGui::Begin(title.c_str(), &window_opened))
		imgui_render();
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)

std::vector<game_object_ptr> scene_system::get_scene_objects() const
{
	std::vector<game_object_ptr> objects;
	for (const auto& weak_object : get_scene().get_objects())
	{
		if (auto object = weak_object.lock())
			objects.emplace_back(std::move(object));
	}
	return objects;
}

void scene_system::register_object(game_object_ptr object)
{
	get_scene().register_transient_object(std::move(object));
}

void scene_system::register_object(collision_object_ptr object)
{
	get_scene().register_transient_object(std::move(object));
}

void scene_system::unregister_object(const game_object_ptr& object)
{
	get_scene().unregister_transient_object(object);
}

core::signals::broadcaster& scene_system::get_broadcaster() const
{
	return get_context().get_broadcaster();
}

core::signals::contactor& scene_system::get_contactor() const
{
	return get_context().get_contactor();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
scene_system_registry::scene_system_registry()
{
	for (auto& [type, function] : get_static_list())
	{
		assert(!functions.contains(type) && "同じ型名のシーンシステムを二重に登録した");
		functions.insert_or_assign(type, function);
	}
}

scene_system_registry::~scene_system_registry()
{
}

std::vector<std::pair<std::string, scene_system_registry::create_function>>& scene_system_registry::get_static_list()
{
	static std::vector<std::pair<std::string, create_function>> list;
	return list;
}

void scene_system_registry::add(std::string_view type, create_function function)
{
	assert(!functions.contains(std::string(type)) && "同じ型名のシーンシステムを二重に登録した");
	functions.insert_or_assign(std::string(type), std::move(function));
}

std::unique_ptr<scene_system> scene_system_registry::create(std::string_view type) const
{
	auto it = functions.find(std::string(type));
	if (it == functions.end())
		return nullptr;
	return it->second();
}

bool scene_system_registry::contains(std::string_view type) const
{
	return functions.find(std::string(type)) != functions.end();
}

std::vector<std::string> scene_system_registry::get_type_names() const
{
	std::vector<std::string> names;
	names.reserve(functions.size());
	for (auto& [type, function] : functions)
		names.emplace_back(type);
	std::sort(names.begin(), names.end());
	return names;
}
