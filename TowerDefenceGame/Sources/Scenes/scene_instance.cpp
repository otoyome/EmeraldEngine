#include "Scenes/scene_instance.h"
#include "Scenes/scene_resource_utility.h"

#include <algorithm>

#include "Object/engine_context.h"
#include "Object/object_manager.h"
#include "Resource/resource_manager.h"

scene_instance::scene_instance(engine_context& context, scene_resource_ptr resource)
: context(context)
, resource(std::move(resource))
{
}

scene_instance::~scene_instance()
{
	uninitialize();
}

bool scene_instance::initialize()
{
	if (!resource || initialized)
		return false;

	//	親と、親に登録されたサブシーンを集める
	std::vector<scene_resource_ptr> resources;
	scene_resource_utility::collect_scenes(resource, resources);

	std::vector<scene_resource_utility::created_scene> created;
	if (!scene_resource_utility::create(resources, created))
		return false;

	for (size_t index = 0; index < resources.size(); ++index)
	{
		loaded_scene& scene = loaded_scenes.emplace_back();
		scene.resource = resources[index];
		scene.system = std::move(created[index].system);
		if (scene.system)
		{
			scene.system->owner = this;
			scene.system->context = &context;
		}
		for (const game_object_ptr& object : created[index].objects)
		{
			scene.objects.emplace_back(object);
			context.get_objects().register_object(object);
		}
	}
	initialized = true;

	//	サブシーンを先に、親を最後に開始する
	for (auto it = loaded_scenes.rbegin(); it != loaded_scenes.rend(); ++it)
	{
		if (it->system)
			it->system->on_begin();
	}
	return true;
}

void scene_instance::uninitialize()
{
	if (!initialized)
		return;
	initialized = false;

	for (loaded_scene& scene : loaded_scenes)
	{
		if (scene.system)
			scene.system->on_end();
	}

	object_manager& objects = context.get_objects();
	for (auto& object : transient_objects)
	{
		if (auto target = object.lock())
			objects.unregister_object(target);
	}
	for (auto& object : collision_objects)
	{
		if (auto target = object.lock())
			objects.unregister_object(target);
	}
	for (loaded_scene& scene : loaded_scenes)
	{
		for (auto& object : scene.objects)
		{
			if (auto target = object.lock())
				objects.unregister_object(target);
		}
	}
	transient_objects.clear();
	collision_objects.clear();
	loaded_scenes.clear();
}

void scene_instance::pre_update(float elapsed_time)
{
	for (loaded_scene& scene : loaded_scenes)
	{
		if (scene.system)
			scene.system->pre_update(elapsed_time);
	}
}

void scene_instance::post_update(float elapsed_time)
{
	for (loaded_scene& scene : loaded_scenes)
	{
		if (scene.system)
			scene.system->post_update(elapsed_time);
	}
}

#if defined(USE_IMGUI)
void scene_instance::imgui_render()
{
	for (loaded_scene& scene : loaded_scenes)
	{
		if (scene.system)
			scene.system->imgui_render_window(scene.resource->get_file_path());
	}
}
#endif	//	defined(USE_IMGUI)

void scene_instance::register_transient_object(game_object_ptr object)
{
	if (!object)
		return;
	object->set_transient(true);
	transient_objects.emplace_back(object);
	context.get_objects().register_object(object);
}

void scene_instance::register_transient_object(collision_object_ptr object)
{
	if (!object)
		return;
	collision_objects.emplace_back(object);
	context.get_objects().register_object(object);
}

void scene_instance::unregister_transient_object(const game_object_ptr& object)
{
	if (!object)
		return;
	std::erase_if(transient_objects, [&](const std::weak_ptr<game_object>& weak_object)
	{
		auto target = weak_object.lock();
		return !target || target == object;
	});
	context.get_objects().unregister_object(object);
}
