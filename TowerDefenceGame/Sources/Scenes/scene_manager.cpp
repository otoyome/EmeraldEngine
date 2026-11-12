#include "Scenes/scene_manager.h"

#include <algorithm>

#include "Scenes/scene_instance.h"
#include "Object/engine_context.h"
#include "Resource/resource_manager.h"

scene_manager::scene_manager(engine_context& context)
: context(context)
{
}

scene_manager::~scene_manager()
{
	remove_all_scene();
}

void scene_manager::updates(float elapsed_time)
{
	std::vector<std::string> removing;
	removing.swap(remove_scenes);
	for (const std::string& name : removing)
	{
		auto it = std::find_if(scenes.begin(), scenes.end(), [&](const auto& scene) { return scene.first == name; });
		if (it == scenes.end())
			continue;
		it->second->uninitialize();
		scenes.erase(it);
	}

	std::vector<add_scene_data> adding;
	adding.swap(add_scenes);
	for (add_scene_data& data : adding)
	{
		auto scene = std::make_unique<scene_instance>(context, data.resource);
		if (!scene->initialize())
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_manager add scene failed[%s]", std::string(data.resource->get_file_path()).c_str());
			continue;
		}
		scenes.emplace_back(data.name, std::move(scene));
	}

	//	予約配信処理
	broadcaster.dispatch_posted();

	for (auto& scene : scenes)
		scene.second->pre_update(elapsed_time);
}

void scene_manager::post_updates(float elapsed_time)
{
	for (auto& scene : scenes)
		scene.second->post_update(elapsed_time);
}

void scene_manager::remove_all_scene()
{
	//	全てのシーンを破棄
	for (auto it = scenes.rbegin(); it != scenes.rend(); ++it)
		it->second->uninitialize();
	scenes.clear();
	add_scenes.clear();
	remove_scenes.clear();
}

bool scene_manager::add_scene(const scene_resource_ptr& resource)
{
	if (!resource)
		return false;

	std::string name = make_scene_name(resource);
	if (!can_add_scene(name))
		return false;

	add_scenes.push_back({ std::move(name), resource });
	return true;
}

bool scene_manager::add_scene(std::string_view file_path)
{
	return add_scene(load_resource(file_path));
}

bool scene_manager::remove_scene(const scene_resource_ptr& resource)
{
	return resource && remove_scene(make_scene_name(resource));
}

bool scene_manager::remove_scene(std::string_view scene_name)
{
	if (std::erase_if(add_scenes, [&](const add_scene_data& data) { return data.name == scene_name; }) > 0)
		return true;

	if (std::find(remove_scenes.begin(), remove_scenes.end(), scene_name) != remove_scenes.end())
		return true;

	if (!find_scene(scene_name))
		return false;

	remove_scenes.emplace_back(scene_name);
	return true;
}

bool scene_manager::valid_scene(const scene_resource_ptr& resource) const
{
	return resource && valid_scene(make_scene_name(resource));
}

bool scene_manager::valid_scene(std::string_view scene_name) const
{
	if (std::find_if(add_scenes.begin(), add_scenes.end(), [&](const add_scene_data& data) { return data.name == scene_name; }) != add_scenes.end())
		return true;

	return find_scene(scene_name) != nullptr;
}

std::vector<std::string> scene_manager::get_valid_scene_names() const
{
	std::vector<std::string> texts;
	for (auto& scene : scenes)
		texts.emplace_back(scene.first);
	for (auto& scene : add_scenes)
		texts.emplace_back(scene.name);
	return texts;
}

scene_instance* scene_manager::find_scene(std::string_view scene_name) const
{
	auto it = std::find_if(scenes.begin(), scenes.end(), [&](const auto& scene) { return scene.first == scene_name; });
	return it != scenes.end() ? it->second.get() : nullptr;
}

std::string scene_manager::make_scene_name(const scene_resource_ptr& resource)
{
	return resource ? resource->get_uuid().to_string() : std::string();
}

scene_resource_ptr scene_manager::load_resource(std::string_view file_path)
{
	if (file_path.empty())
		return nullptr;

	auto operation = resource_manager::instance()->load<scene_resource>(file_path);
	if (!operation || !operation->is_succeeded())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_manager load failed[%s]", std::string(file_path).c_str());
		return nullptr;
	}
	return operation->get_resource();
}

bool scene_manager::can_add_scene(std::string_view name) const
{
	const bool removing = std::find(remove_scenes.begin(), remove_scenes.end(), name) != remove_scenes.end();
	if (!removing && find_scene(name))
		return false;

	return std::find_if(add_scenes.begin(), add_scenes.end(), [&](const add_scene_data& data) { return data.name == name; }) == add_scenes.end();
}


#if defined(USE_IMGUI)
void scene_manager::imgui_render()
{
	//	@tool 窓に映すワールド（プレビュー窓）は scenes 窓が無いので、システムの表示をそのまま呼ぶ
	if (context.is_embedded())
	{
		for (auto& scene : scenes)
		{
			for (const auto& loaded : scene.second->get_loaded_scenes())
			{
				if (loaded.system)
					loaded.system->imgui_render();
			}
		}
		return;
	}

	//	@tool 生きているシーンの一覧（サブシーンは字下げ）。システムの窓はボタンで開く
	if (ImGui::Begin(context.make_window_name("scenes").c_str()))
	{
		for (auto& scene : scenes)
		{
			const auto& loaded_scenes = scene.second->get_loaded_scenes();
			for (size_t index = 0; index < loaded_scenes.size(); ++index)
			{
				const scene_instance::loaded_scene& loaded = loaded_scenes[index];
				ImGui::PushID(&loaded);
				if (index > 0)
					ImGui::Indent();
				const cache_string path(loaded.resource->get_file_path());
				ImGui::TextUnformatted(path.get<utf8_cache>().data());
				if (loaded.system)
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("system"))
						loaded.system->open_window();
				}
				if (index > 0)
					ImGui::Unindent();
				ImGui::PopID();
			}
		}
	}
	ImGui::End();

	for (auto& scene : scenes)
		scene.second->imgui_render();
}
#endif	//	defined(USE_IMGUI)
