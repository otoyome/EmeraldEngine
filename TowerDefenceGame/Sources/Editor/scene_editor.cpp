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
#include "Scenes/scene_set_resource.h"
#include "Scenes/scene_instance.h"
#include "Scenes/scene_system.h"
#include "Object/game_object_registry.h"
#include "Object/spatial_object.h"

#include <unordered_set>

scene_editor::scene_editor()
: edit_world(create_world())
{
	edit_world->get_objects().set_pause(true);
	edit_world->get_context().set_editing(true);
}

scene_editor::~scene_editor()
{
	handlers = {};
	playing_scenes.clear();
	close();
	edit_world.reset();
}

void scene_editor::request_edit(std::string_view file_path)
{
	edit_request.clear();
	add_requests.clear();

	//	シーンセットなら中のシーンを全部開く
	if (scene_set_resource::is_scene_set_path(file_path))
	{
		auto operation = resource_manager::instance()->load<scene_set_resource>(file_path);
		if (!operation || !operation->is_succeeded())
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor scene set load failed[%s]", std::string(file_path).c_str());
			return;
		}
		const std::vector<std::string> paths = operation->get_resource()->get_scene_paths();
		if (paths.empty())
		{
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "scene_editor scene set is empty[%s]", std::string(file_path).c_str());
			return;
		}
		edit_request = paths.front();
		add_requests.assign(std::next(paths.begin()), paths.end());
		return;
	}
	edit_request = file_path;
}

void scene_editor::request_add(std::string_view file_path)
{
	add_requests.emplace_back(file_path);
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

	//	足すシーンは開いた後に読む
	if (!add_requests.empty())
	{
		const std::vector<std::string> paths = std::move(add_requests);
		add_requests.clear();
		for (const std::string& path : paths)
			add_scene(path);
	}

	if (play_request)
	{
		play_request = false;
		if (is_editing())
		{
			stop_play();
			apply_contents();
			start_play(get_edited_scenes(), {});
		}
	}

	if (!play_file_request.empty())
	{
		const std::string file_path = std::move(play_file_request);
		std::string open_file = std::move(play_open_file_request);
		play_file_request.clear();
		play_open_file_request.clear();

		//	同じシーンを再生中なら何もしない
		const bool playing_same = playing_scenes.size() == 1 && playing_scenes.front()->get_file_path() == file_path;
		if (!playing_same)
		{
			auto operation = resource_manager::instance()->load<scene_resource>(file_path);
			if (operation && operation->is_succeeded())
			{
				stop_play();
				start_play({ operation->get_resource() }, open_file);
			}
			else
			{
				DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor load failed[%s]", file_path.c_str());
			}
		}

		//	開いてもらうファイルは再生中のワールドに渡す
		else if (!open_file.empty() && handlers.open_file)
			handlers.open_file(open_file);
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

		if (!scene.resource->set_contents(objects, scene.system.get()))
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

	if (!create_edited_scenes({ operation->get_resource() }))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor create failed[%s]", std::string(file_path).c_str());
		return false;
	}
	return true;
}

void scene_editor::close()
{
	for (edited_scene& scene : edited_scenes)
		release_edited_scene(scene);
	edited_scenes.clear();
	current_scene.reset();
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

bool scene_editor::add_scene(std::string_view file_path)
{
	if (!is_editing() || is_playing())
		return false;

	auto operation = resource_manager::instance()->load<scene_resource>(file_path);
	if (!operation || !operation->is_succeeded())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor add scene load failed[%s]", std::string(file_path).c_str());
		return false;
	}

	//	既に開いているシーンは追加しない
	const scene_resource_ptr resource = operation->get_resource();
	for (const edited_scene& scene : edited_scenes)
	{
		if (scene.resource == resource)
			return false;
	}

	if (!create_edited_scenes({ resource }))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor create failed[%s]", std::string(file_path).c_str());
		return false;
	}
	return true;
}

void scene_editor::remove_scene(const scene_resource_ptr& resource)
{
	if (!can_remove_scene() || !resource)
		return;

	auto it = std::find_if(edited_scenes.begin(), edited_scenes.end(), [&](const edited_scene& scene) { return scene.resource == resource; });
	if (it == edited_scenes.end())
		return;
	release_edited_scene(*it);
	edited_scenes.erase(it);
	if (current_scene == resource)
		current_scene.reset();
}

void scene_editor::imgui_render()
{
	if (!is_editing() || is_playing())
		return;

	//	オブジェクトを選んでいる間は、そのシーンを置く先のシーンにしておく
	if (const edited_scene* scene = find_edited_scene(edit_world->get_context().get_selected_object()))
		current_scene = scene->resource;

	imgui_render_edit_window();
}

std::vector<scene_resource_ptr> scene_editor::get_edited_scenes() const
{
	std::vector<scene_resource_ptr> result;
	result.reserve(edited_scenes.size());
	for (const edited_scene& scene : edited_scenes)
		result.emplace_back(scene.resource);
	return result;
}

void scene_editor::set_current_scene(const scene_resource_ptr& resource)
{
	//	シーンを選んだら、オブジェクトの選択は外す
	current_scene = resource;
	edit_world->get_context().set_selected_object(nullptr);
}

std::vector<std::string> scene_editor::get_addable_scenes() const
{
	std::vector<std::string> result;
	if (!asset_registry::is_valid())
		return result;
	for (const std::string& scene_path : asset_registry::instance()->find_paths(resource::asset_types::scene))
	{
		const bool loaded = std::any_of(edited_scenes.begin(), edited_scenes.end(), [&](const edited_scene& edited) { return edited.resource->get_file_path() == scene_path; });
		if (!loaded)
			result.emplace_back(scene_path);
	}
	return result;
}

std::vector<std::shared_ptr<game_object>> scene_editor::get_scene_objects(const scene_resource_ptr& resource) const
{
	std::vector<game_object_ptr> result;
	for (const edited_scene& scene : edited_scenes)
	{
		if (scene.resource != resource)
			continue;
		for (const auto& weak_object : scene.objects)
		{
			if (auto object = weak_object.lock(); object && !object->is_transient())
				result.emplace_back(std::move(object));
		}
	}
	return result;
}

scene_resource_ptr scene_editor::find_object_scene(const std::shared_ptr<game_object>& object)
{
	const edited_scene* scene = find_edited_scene(object);
	return scene ? scene->resource : nullptr;
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

	//	シーンの編集窓
	const cache_string path(scene.resource->get_file_path());
	const std::string title = scene_resource_utility::make_display_name(path.get<utf8_cache>()) + " (editing)###scene_editor_edit_window";
	if (ImGui::Begin(title.c_str(), &edit_window_opened))
		imgui_render_scene_settings(scene.resource);
	ImGui::End();
}

void scene_editor::imgui_render_scene_settings(const scene_resource_ptr& resource)
{
	auto it = std::find_if(edited_scenes.begin(), edited_scenes.end(), [&](const edited_scene& edited) { return edited.resource == resource; });
	if (it == edited_scenes.end())
		return;
	edited_scene& scene = *it;

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

		//	システムの設定
		auto current_it = std::find_if(edited_scenes.begin(), edited_scenes.end(), [&](const edited_scene& edited) { return edited.resource == resource; });
		if (current_it != edited_scenes.end() && current_it->system)
		{
			ImGui::SeparatorText("settings");
			current_it->system->imgui_render();
		}
	}
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

std::shared_ptr<game_object> scene_editor::place_object(std::string_view type, const std::optional<math::vector3d>& position)
{
	if (!is_editing())
		return nullptr;

	const std::shared_ptr<game_object> selected = edit_world->get_context().get_selected_object();
	if (!position && find_edited_scene(selected))
		return place_object_as_child(type, selected);
	return add_object(get_current_scene(), type, position ? *position : get_placement_position());
}

std::shared_ptr<game_object> scene_editor::place_object_in_scene(std::string_view type, const scene_resource_ptr& resource, const std::optional<math::vector3d>& position)
{
	for (edited_scene& scene : edited_scenes)
	{
		if (scene.resource == resource)
			return add_object(scene, type, position ? *position : get_placement_position());
	}
	return nullptr;
}

std::shared_ptr<game_object> scene_editor::place_object_as_child(std::string_view type, const std::shared_ptr<game_object>& parent)
{
	edited_scene* scene = find_edited_scene(parent);
	if (!scene || parent->is_transient())
		return nullptr;

	//	置いた位置のまま親に付ける
	const game_object_ptr object = add_object(*scene, type, get_placement_position());
	if (object && parent->check_type(spatial_object::type_name()))
		spatial_object::attach_object(object, parent);
	return object;
}

scene_editor::edited_scene& scene_editor::get_current_scene()
{
	//	オブジェクトを選んでいる時は、そのオブジェクトのシーン
	if (edited_scene* scene = find_edited_scene(edit_world->get_context().get_selected_object()))
		return *scene;

	for (edited_scene& scene : edited_scenes)
	{
		if (scene.resource == current_scene)
			return scene;
	}
	return edited_scenes.front();
}

std::shared_ptr<game_object> scene_editor::add_object(edited_scene& scene, std::string_view type, const math::vector3d& position)
{
	if (is_playing() || !game_object_registry::is_valid())
		return nullptr;

	game_object_ptr object = game_object_registry::instance()->create(type);
	if (!object)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_editor add object failed[%s]", std::string(type).c_str());
		return nullptr;
	}

	object->set_name(make_unique_object_name(type));
	if (object->check_type(spatial_object::type_name()))
		static_cast<spatial_object*>(object.get())->set_world_translation(position);

	scene.objects.emplace_back(object);
	edit_world->get_objects().register_object(object);
	edit_world->get_context().set_selected_object(object);
	return object;
}

bool scene_editor::can_remove_object(const std::shared_ptr<game_object>& target) const
{
	if (is_playing() || !target || target->is_transient())
		return false;

	for (const edited_scene& scene : edited_scenes)
	{
		for (const auto& weak_object : scene.objects)
		{
			if (weak_object.lock() == target)
				return true;
		}
	}
	return false;
}

bool scene_editor::remove_object(const std::shared_ptr<game_object>& target)
{
	if (!can_remove_object(target))
		return false;

	//	子孫のオブジェクトも一緒に消す
	const std::vector<game_object_ptr> remove_objects = collect_object_tree(target);
	std::unordered_set<const game_object*> removes;
	for (const game_object_ptr& object : remove_objects)
		removes.insert(object.get());

	//	ワールドから外し、シーンのオブジェクトからも外す
	object_manager& objects = edit_world->get_objects();
	for (const game_object_ptr& object : remove_objects)
		objects.unregister_object(object);
	for (edited_scene& scene : edited_scenes)
	{
		std::erase_if(scene.objects, [&](const std::weak_ptr<game_object>& weak_object)
		{
			const game_object_ptr object = weak_object.lock();
			return object && removes.find(object.get()) != removes.end();
		});
	}

	edit_world->get_context().set_selected_object(nullptr);
	return true;
}

bool scene_editor::attach_object(const std::shared_ptr<game_object>& child, const std::shared_ptr<game_object>& parent)
{
	if (is_playing() || !child || child->is_transient())
		return false;
	edited_scene* child_scene = find_edited_scene(child);
	if (!child_scene)
		return false;
	edited_scene* parent_scene = child_scene;
	if (parent)
	{
		if (parent->is_transient())
			return false;
		parent_scene = find_edited_scene(parent);
		if (!parent_scene)
			return false;
	}

	if (!spatial_object::attach_object(child, parent))
		return false;

	//	親子は同じシーンのファイルの中でしか保存されないので、親が別のシーンなら子孫ごと親のシーンへ移す
	if (parent_scene != child_scene)
		move_object_tree(child, *parent_scene);
	return true;
}

bool scene_editor::move_object_to_scene(const std::shared_ptr<game_object>& object, const scene_resource_ptr& resource)
{
	if (is_playing() || !object || object->is_transient())
		return false;
	edited_scene* from_scene = find_edited_scene(object);
	if (!from_scene)
		return false;
	auto it = std::find_if(edited_scenes.begin(), edited_scenes.end(), [&](const edited_scene& scene) { return scene.resource == resource; });
	if (it == edited_scenes.end())
		return false;

	//	親は別のシーンに残るので外してから移す
	if (object->check_type(spatial_object::type_name()))
		spatial_object::attach_object(object, nullptr);
	if (&*it != from_scene)
		move_object_tree(object, *it);
	return true;
}

void scene_editor::move_object_tree(const std::shared_ptr<game_object>& target, edited_scene& scene)
{
	const std::vector<game_object_ptr> moves = collect_object_tree(target);
	std::unordered_set<const game_object*> move_set;
	for (const game_object_ptr& object : moves)
		move_set.insert(object.get());
	for (edited_scene& edited : edited_scenes)
	{
		std::erase_if(edited.objects, [&](const std::weak_ptr<game_object>& weak_object)
		{
			const game_object_ptr object = weak_object.lock();
			return object && move_set.find(object.get()) != move_set.end();
		});
	}
	for (const game_object_ptr& object : moves)
		scene.objects.emplace_back(object);
}

scene_editor::edited_scene* scene_editor::find_edited_scene(const std::shared_ptr<game_object>& object)
{
	if (!object)
		return nullptr;
	for (edited_scene& scene : edited_scenes)
	{
		for (const auto& weak_object : scene.objects)
		{
			if (weak_object.lock() == object)
				return &scene;
		}
	}
	return nullptr;
}

std::vector<std::shared_ptr<game_object>> scene_editor::collect_object_tree(const std::shared_ptr<game_object>& target) const
{
	//	空間の親子をたどって、子孫のオブジェクトも集める
	std::unordered_map<const hierarchy*, game_object_ptr> owners;
	for (const game_object_ptr& object : edit_world->get_objects().search_game_objects<spatial_object>())
		owners.emplace(static_cast<spatial_object*>(object.get())->get_spatial().get(), object);

	std::unordered_set<const game_object*> visited;
	std::vector<game_object_ptr> result;
	auto add_object = [&](const game_object_ptr& object)
	{
		if (object && !object->is_transient() && visited.insert(object.get()).second)
			result.emplace_back(object);
	};
	std::function<void(const hierarchy*)> collect_children = [&](const hierarchy* node)
	{
		for (const auto& child : node->get_children())
		{
			auto it = owners.find(child.get());
			if (it != owners.end())
				add_object(it->second);
			collect_children(child.get());
		}
	};
	add_object(target);
	if (target && target->check_type(spatial_object::type_name()))
		collect_children(static_cast<spatial_object*>(target.get())->get_spatial().get());
	return result;
}

std::string scene_editor::make_unique_object_name(std::string_view base) const
{
	std::unordered_set<std::string> names;
	for (const game_object_ptr& object : edit_world->get_objects().search_game_objects<game_object>())
		names.emplace(object->get_name());

	std::string name(base);
	for (int number = 1; names.find(name) != names.end(); ++number)
		name = std::string(base) + "_" + std::to_string(number);
	return name;
}

math::vector3d scene_editor::get_placement_position() const
{
	//	エディターカメラの前方 5m（無ければ原点）
	constexpr double placement_distance = system_units::convert_m_to_unit(5.0);
	const std::shared_ptr<editor_camera_object> camera = edit_world->get_objects().search_object<editor_camera_object>();
	if (!camera)
		return math::vector3d(0.0, 0.0, 0.0);

	const math::vector3f front = camera->get_rotation().front();
	return camera->get_position() + math::vector3d(front.x * placement_distance, front.y * placement_distance, front.z * placement_distance);
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

void scene_editor::start_play(const std::vector<scene_resource_ptr>& resources, std::string_view open_file)
{
	playing_scenes = resources;
	if (handlers.start)
		handlers.start(resources, open_file);
}

void scene_editor::stop_play()
{
	if (playing_scenes.empty())
		return;
	playing_scenes.clear();
	if (handlers.stop)
		handlers.stop();
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
