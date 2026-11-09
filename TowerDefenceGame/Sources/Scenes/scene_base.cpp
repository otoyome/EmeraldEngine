#include "Scenes/scene_base.h"
#include "Object/engine_context.h"

scene_base::scene_base()
: context(constructing_context)
{
}

scene_base::~scene_base()
{
}

bool scene_base::initialize(engine_context& context)
{
	this->context = &context;
	return initialize_core();
}

bool scene_base::uninitialize()
{
	for (auto& object : game_objects)
	{
		if (auto obj = object.lock())
			get_context().get_objects().unregister_object(obj);
	}
	for (auto& object : collision_objects)
	{
		if (auto obj = object.lock())
			get_context().get_objects().unregister_object(obj);
	}
	return uninitialize_core();
}

engine_context& scene_base::get_context() const
{
	assert(context && "scene_manager を通さずに作ったシーンからワールドを参照した");
	return *context;
}

void scene_base::update(float elapsed_time)
{
	update_core(elapsed_time);
}

#ifdef USE_IMGUI
void scene_base::imgui_render()
{
	imgui_render_core();
}
#endif

signals::broadcaster& scene_base::get_broadcaster()
{
	return get_context().get_scenes().get_broadcaster();
}

signals::contactor& scene_base::get_contactor()
{
	return get_context().get_scenes().get_contactor();
}

void scene_base::register_object(game_object_ptr object)
{
	game_objects.emplace_back(object);
	get_context().get_objects().register_object(object);
}

void scene_base::register_object(collision_object_ptr object)
{
	collision_objects.emplace_back(object);
	get_context().get_objects().register_object(object);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	for (auto& remove : remove_scenes)
	{
		auto scene = scenes.find(remove);
		scene->second->uninitialize();
		scenes.erase(scene);
	}

	if (!add_scenes.empty())
	{
		std::sort(add_scenes.begin(), add_scenes.end(), [&](add_scene_data& l, add_scene_data& r){return l.order < r.order;});
		for (auto& add_scene : add_scenes)
		{
			add_scene.func();
		}
	}
	remove_scenes.clear();
	add_scenes.clear();

	//	予約配信処理
	broadcaster.dispatch_posted();

	for (auto& scene : scenes)
	{
		scene.second->update(elapsed_time);
	}
}

void scene_manager::remove_all_scene()
{
	//	全てのシーンを破棄
	for (auto& scene : scenes)
	{
		scene.second->uninitialize();
	}
	scenes.clear();
}

std::vector<std::string_view> scene_manager::get_valid_scene_names() const
{
	std::vector<std::string_view> texts;
	for (auto& scene : scenes)
	{
		texts.emplace_back(scene.first);
	}
	for (auto& scene : add_scenes)
	{
		texts.emplace_back(scene.name);
	}

	return texts;
}

bool scene_manager::remove_scene(std::string_view scene_name)
{
	if (std::find_if(remove_scenes.begin(),
		remove_scenes.end(),
		[&](std::string_view name) {return name == scene_name; }) != remove_scenes.end())
	{
		return true;
	}

	if (scenes.find(scene_name) == scenes.end())
		return false;

	remove_scenes.emplace_back(scene_name);
	return true;
}

bool scene_manager::valid_scene(std::string_view scene_name)
{
	if (std::find_if(add_scenes.begin(), add_scenes.end(),
					 [&](add_scene_data& data) {return data.name == scene_name; }) != add_scenes.end())
		return true;

	return scenes.find(scene_name) != scenes.end();
}

#if defined(USE_IMGUI)
void scene_manager::imgui_render()
{
	if (ImGui::Begin(context.make_window_name("scenes").c_str()))
	{
		for (auto& scene : scenes)
		{
			scene.second->imgui_render();
		}
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)