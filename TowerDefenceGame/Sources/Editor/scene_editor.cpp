#include "Editor/scene_editor.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <filesystem>

#include "Editor/editor_camera.h"
#include "Resource/asset_registry.h"
#include "System/asset_browser.h"
#include "Object/world.h"
#include "Object/object_manager.h"
#include "Resource/resource_manager.h"
#include "Scenes/scene_manager.h"
#include "Scenes/scene_resource_utility.h"
#include "Scenes/scene_instance.h"
#include "Scenes/scene_system.h"

scene_editor::scene_editor()
: edit_world(create_world())
{
	edit_world->get_objects().set_pause(true);
	edit_world->get_context().set_editing(true);
}

scene_editor::~scene_editor()
{
	play_world.reset();
	close();
	edit_world.reset();
}

void scene_editor::request_edit(std::string_view file_path)
{
	edit_request = file_path;
}

void scene_editor::request_play()
{
	play_request = true;
	play_file_request.clear();
}

void scene_editor::request_play(std::string_view file_path, std::string_view open_file)
{
	play_file_request = file_path;
	play_open_file_request = open_file;
	play_request = false;
}

void scene_editor::request_stop()
{
	stop_request = true;
	play_request = false;
	play_file_request.clear();
}

void scene_editor::apply_requests()
{
	if (stop_request)
	{
		stop_request = false;
		stop_play();
	}

	if (!edit_request.empty())
	{
		const std::string file_path = std::move(edit_request);
		edit_request.clear();
		stop_play();
		open(file_path);
	}

	if (play_request)
	{
		play_request = false;
		if (is_editing())
		{
			stop_play();
			apply_contents();
			start_play(edited_scenes.front().resource);
		}
	}

	if (!play_file_request.empty())
	{
		const std::string file_path = std::move(play_file_request);
		std::string open_file = std::move(play_open_file_request);
		play_file_request.clear();
		play_open_file_request.clear();

		//	同じシーンを再生中なら何もしない
		bool playing_same = false;
		if (play_world)
		{
			for (const auto& scene : play_world->get_scenes().get_scenes())
				playing_same |= scene.second->get_resource()->get_file_path() == file_path;
		}
		if (!playing_same)
		{
			auto operation = resource_manager::instance()->load<scene_resource>(file_path);
			if (operation && operation->is_succeeded())
			{
				stop_play();
				start_play(operation->get_resource());
			}
			else
			{
				DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor load failed[%s]", file_path.c_str());
			}
		}

		//	開いてもらうファイルは再生中のワールドに渡す
		if (play_world && !open_file.empty())
			play_world->get_context().set_open_file(std::move(open_file));
	}
}

bool scene_editor::apply_contents()
{
	bool result = true;
	for (const edited_scene& scene : edited_scenes)
	{
		std::vector<game_object_ptr> objects;
		for (const auto& weak_object : scene.objects)
		{
			if (auto object = weak_object.lock(); object && !object->is_transient())
				objects.emplace_back(std::move(object));
		}

		//	開いたシーンは編集中のサブシーンリストで保存する
		const auto& scene_list = (&scene == &edited_scenes.front()) ? sub_scenes : scene.resource->get_sub_scenes();
		if (!scene.resource->set_contents(scene_list, objects, scene.system.get()))
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor apply failed[%s]", std::string(scene.resource->get_file_path()).c_str());
			result = false;
		}
	}
	return result;
}

bool scene_editor::save()
{
	if (!is_editing() || !apply_contents())
		return false;

	bool result = true;
	for (const edited_scene& scene : edited_scenes)
	{
		if (!resource_manager::instance()->save(scene.resource))
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor save failed[%s]", std::string(scene.resource->get_file_path()).c_str());
			result = false;
		}
	}
	return result;
}

world& scene_editor::get_active_world()
{
	return play_world ? *play_world : *edit_world;
}

scene_resource_ptr scene_editor::get_edit_resource() const
{
	return is_editing() ? edited_scenes.front().resource : nullptr;
}

void scene_editor::imgui_menu()
{
	if (is_playing())
	{
		if (ImGui::MenuItem("Stop"))
			request_stop();
	}
	else
	{
		if (ImGui::MenuItem("Play", nullptr, false, is_editing()))
			request_play();
	}
	if (ImGui::MenuItem("Save", nullptr, false, is_editing() && !is_playing()))
		save();

	if (const scene_resource_ptr resource = get_edit_resource())
	{
		const cache_string file_path(resource->get_file_path());
		ImGui::TextDisabled("%s%s", file_path.get<utf8_cache>().data(), is_playing() ? " (playing)" : "");
	}
}

bool scene_editor::open(std::string_view file_path)
{
	close();

	auto operation = resource_manager::instance()->load<scene_resource>(file_path);
	if (!operation || !operation->is_succeeded())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor load failed[%s]", std::string(file_path).c_str());
		return false;
	}

	std::vector<scene_resource_ptr> resources;
	scene_resource_utility::collect_scenes(operation->get_resource(), resources);
	if (!create_edited_scenes(resources))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor create failed[%s]", std::string(file_path).c_str());
		return false;
	}
	sub_scenes = operation->get_resource()->get_sub_scenes();
	return true;
}

void scene_editor::close()
{
	for (edited_scene& scene : edited_scenes)
		release_edited_scene(scene);
	edited_scenes.clear();
	sub_scenes.clear();
}

bool scene_editor::create_edited_scenes(const std::vector<scene_resource_ptr>& resources)
{
	std::vector<scene_resource_utility::created_scene> created;
	if (!scene_resource_utility::create(resources, created))
		return false;

	object_manager& objects = edit_world->get_objects();
	for (size_t index = 0; index < resources.size(); ++index)
	{
		edited_scene& scene = edited_scenes.emplace_back();
		scene.resource = resources[index];
		scene.system = std::move(created[index].system);
		if (scene.system)
			scene.system->context = &edit_world->get_context();
		for (const game_object_ptr& object : created[index].objects)
		{
			scene.objects.emplace_back(object);
			objects.register_object(object);
		}
	}
	return true;
}

void scene_editor::release_edited_scene(edited_scene& scene)
{
	object_manager& objects = edit_world->get_objects();
	for (auto& weak_object : scene.objects)
	{
		if (auto object = weak_object.lock())
			objects.unregister_object(object);
	}
	scene.objects.clear();
}

bool scene_editor::add_sub_scene(std::string_view file_path)
{
	if (!is_editing() || is_playing())
		return false;

	auto operation = resource_manager::instance()->load<scene_resource>(file_path);
	if (!operation || !operation->is_succeeded())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor sub scene load failed[%s]", std::string(file_path).c_str());
		return false;
	}

	//	開いたシーン自身と、既に読み込んでいるシーンは追加しない
	const scene_resource_ptr resource = operation->get_resource();
	for (const edited_scene& scene : edited_scenes)
	{
		if (scene.resource == resource)
			return false;
	}
	if (!resource->get_sub_scenes().empty())
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "scene_editor nested sub scenes are ignored[%s]", std::string(file_path).c_str());

	if (!create_edited_scenes({ resource }))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor create failed[%s]", std::string(file_path).c_str());
		return false;
	}
	sub_scenes.emplace_back(resource);
	return true;
}

void scene_editor::remove_sub_scene(const scene_resource_ptr& resource)
{
	if (!is_editing() || is_playing() || !resource)
		return;

	//	開いたシーンは外さない
	for (auto it = std::next(edited_scenes.begin()); it != edited_scenes.end(); ++it)
	{
		if (it->resource != resource)
			continue;
		release_edited_scene(*it);
		edited_scenes.erase(it);
		break;
	}
	std::erase_if(sub_scenes, [&](const resource::asset_handle<scene_resource>& handle)
	{
		return handle.get() == resource || handle.get_reference().uuid == resource->get_uuid();
	});
}

void scene_editor::imgui_render()
{
	if (!is_editing() || is_playing())
		return;

	//	編集中は scenes 窓に編集中のシーン（親とサブシーン）を出す（再生中は再生用のワールドの scenes 窓）
	if (ImGui::Begin(edit_world->get_context().make_window_name("scenes").c_str()))
	{
		for (size_t index = 0; index < edited_scenes.size(); ++index)
		{
			edited_scene& scene = edited_scenes[index];
			ImGui::PushID(scene.resource.get());
			if (index > 0)
				ImGui::Indent();
			const cache_string path(scene.resource->get_file_path());
			ImGui::TextUnformatted(path.get<utf8_cache>().data());
			ImGui::SameLine();
			if (ImGui::SmallButton("edit"))
			{
				edit_target = scene.resource;
				edit_window_opened = true;
			}
			if (index > 0)
				ImGui::Unindent();
			ImGui::PopID();
		}

		//	読み込めなかったサブシーンはパスだけ出す
		for (const auto& sub_scene : sub_scenes)
		{
			const resource::asset_reference<scene_resource> reference = sub_scene.get_reference();
			const bool loaded = std::any_of(edited_scenes.begin(), edited_scenes.end(), [&](const edited_scene& scene) { return scene.resource->get_uuid() == reference.uuid; });
			if (loaded)
				continue;
			const cache_string path(reference.path);
			ImGui::Indent();
			ImGui::TextDisabled("%s (not loaded)", path.get<utf8_cache>().data());
			ImGui::Unindent();
		}
	}
	ImGui::End();

	imgui_render_edit_window();
}

void scene_editor::imgui_render_edit_window()
{
	if (!edit_window_opened)
		return;

	auto it = std::find_if(edited_scenes.begin(), edited_scenes.end(), [&](const edited_scene& scene) { return scene.resource == edit_target; });
	if (it == edited_scenes.end())
	{
		edit_window_opened = false;
		edit_target.reset();
		return;
	}
	edited_scene& scene = *it;
	const bool is_root = (it == edited_scenes.begin());

	//	シーンの編集窓
	const cache_string path(scene.resource->get_file_path());
	const std::string title = std::string(path.get<utf8_cache>()) + " (editing)###scene_editor_edit_window";
	if (ImGui::Begin(title.c_str(), &edit_window_opened))
	{
		//	システムの付け替え
		ImGui::SeparatorText("system");
		const std::string current = scene.system ? std::string(scene.system->get_type_name()) : std::string("(none)");
		if (ImGui::BeginCombo("type", current.c_str()))
		{
			if (ImGui::Selectable("(none)", !scene.system))
				set_system(scene, {});
			if (scene_system_registry::is_valid())
			{
				for (const std::string& type : scene_system_registry::instance()->get_type_names())
				{
					if (ImGui::Selectable(type.c_str(), type == current))
						set_system(scene, type);
				}
			}
			ImGui::EndCombo();
		}

		//	サブシーンの登録
		if (is_root)
		{
			ImGui::SeparatorText("sub scenes");

			//	サブシーンリスト
			scene_resource_ptr remove_target;
			for (size_t index = 0; index < sub_scenes.size(); ++index)
			{
				const resource::asset_reference<scene_resource> reference = sub_scenes[index].get_reference();
				const cache_string sub_path(reference.path);
				ImGui::PushID(static_cast<int>(index));
				if (ImGui::SmallButton("remove"))
				{
					for (const edited_scene& edited : edited_scenes)
					{
						if (edited.resource->get_uuid() == reference.uuid)
							remove_target = edited.resource;
					}
					if (!remove_target)
					{
						sub_scenes.erase(sub_scenes.begin() + static_cast<std::ptrdiff_t>(index));
						ImGui::PopID();
						break;
					}
				}
				ImGui::SameLine();
				ImGui::TextUnformatted(sub_path.get<utf8_cache>().data());
				ImGui::PopID();
			}
			if (remove_target)
				remove_sub_scene(remove_target);

			//	シーンリソースをアセットブラウザからドロップするか、一覧から選んで追加する
			ImGui::Button("drop .escene here to add");
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(asset_browser::drag_payload_type))
				{
					const std::string file_path(static_cast<const char*>(payload->Data));
					if (std::filesystem::path(file_path).extension().string() == "." + std::string(scene_resource::extension))
						add_sub_scene(file_path);
				}
				ImGui::EndDragDropTarget();
			}
			if (asset_registry::is_valid() && ImGui::BeginCombo("add sub scene", nullptr, ImGuiComboFlags_NoPreview))
			{
				for (const std::string& scene_path : asset_registry::instance()->find_paths(resource::asset_types::scene))
				{
					const bool loaded = std::any_of(edited_scenes.begin(), edited_scenes.end(), [&](const edited_scene& edited) { return edited.resource->get_file_path() == scene_path; });
					if (loaded)
						continue;
					const cache_string label(scene_path);
					if (ImGui::Selectable(label.get<utf8_cache>().data()))
						add_sub_scene(scene_path);
				}
				ImGui::EndCombo();
			}
		}

		//	システムの設定
		auto current_it = std::find_if(edited_scenes.begin(), edited_scenes.end(), [&](const edited_scene& edited) { return edited.resource == edit_target; });
		if (current_it != edited_scenes.end() && current_it->system)
		{
			ImGui::SeparatorText("settings");
			current_it->system->imgui_render();
		}
	}
	ImGui::End();
}

void scene_editor::set_system(edited_scene& scene, std::string_view type)
{
	if (is_playing())
		return;

	std::unique_ptr<scene_system> system;
	if (!type.empty())
	{
		system = scene_system_registry::is_valid() ? scene_system_registry::instance()->create(type) : nullptr;
		if (!system)
		{
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "scene_editor unknown system type[%s]", std::string(type).c_str());
			return;
		}
		system->context = &edit_world->get_context();
	}
	scene.system = std::move(system);
}

void scene_editor::editor_update(float elapsed_time)
{
	if (is_playing())
		return;

	for (edited_scene& scene : edited_scenes)
	{
		if (scene.system)
			scene.system->editor_update(elapsed_time);
	}
}

void scene_editor::start_play(const scene_resource_ptr& resource)
{
	play_world = create_world();
	play_world->get_scenes().add_scene(resource);
}

void scene_editor::stop_play()
{
	play_world.reset();
}

std::unique_ptr<world> scene_editor::create_world()
{
	auto result = std::make_unique<world>();
	//	エディターのビューポートの窓に映す
	result->get_context().set_embedded(true);
	auto editor_camera = std::make_shared<editor_camera_object>("エディターカメラ", math::vector3d(0.0, 0.0, 0.0), math::rotatorf(0.0f, 0.0f, 0.0f), 0.25f);
	editor_camera->set_transient(true);
	result->get_objects().register_object(editor_camera);
	return result;
}
#endif	//	defined(USE_IMGUI)
