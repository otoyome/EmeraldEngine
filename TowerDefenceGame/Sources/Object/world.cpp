#include "Object/world.h"
#include "Object/object_manager.h"
#include "Object/camera_object.h"
#include "Object/light_object.h"
#include "Object/model_object.h"
#include "Object/post_process_volume_object.h"
#include "Scenes/scene_base.h"
#include "effect_instance.h"

world::world()
: effects(std::make_unique<effect_manager>())
, objects(std::make_unique<object_manager>())
, scenes(std::make_unique<scene_manager>())
{
}

world::~world()
{
	scenes.reset();
	if (bound_main_world)
		scene_manager::set_main_world_instance(nullptr);

	objects->finalize();
	objects.reset();
	if (bound_main_world)
		object_manager::set_main_world_instance(nullptr);

	effects.reset();
	if (bound_main_world)
		effect_manager::set_main_world_instance(nullptr);

	if (main_world == this)
		main_world = nullptr;
}

void world::bind_main_world()
{
	effect_manager::set_main_world_instance(effects.get());
	object_manager::set_main_world_instance(objects.get());
	scene_manager::set_main_world_instance(scenes.get());
	bound_main_world = true;
	main_world = this;
}

camera_ptr world::find_view_camera()
{
	camera_ptr result;
	std::uint64_t result_order = 0;
	for (const auto& object : objects->search_objects<camera_object>())
	{
		if (!object->is_active())
			continue;

		const camera_ptr& source = object->get_camera();
		if (!source || !source->is_active())
			continue;

		if (!result || object->get_view_order() > result_order)
		{
			result = source;
			result_order = object->get_view_order();
		}
	}
	return result;
}

void world::collect_lights(light_container& container, const math::vector3d& render_origin, const light_view_info& view)
{
	collected_lights.clear();
	for (const auto& object : objects->search_objects<light_object>())
	{
		if (light_ptr source = object->get_light())
			collected_lights.emplace_back(std::move(source));
	}

	collected_light_probes.clear();
	for (const auto& object : objects->search_objects<light_probe_object>())
	{
		if (light_probe_ptr source = object->get_light_probe())
			collected_light_probes.emplace_back(std::move(source));
	}

	lights_collector.collect(container, collected_lights, collected_light_probes, render_origin, view);
}

void world::collect_post_process_volumes(post_process_volume_container& container, const math::vector3d& view_position)
{
	collected_volumes.clear();
	for (const auto& object : objects->search_objects<post_process_volume_object>())
	{
		if (post_process_volume_ptr source = object->get_volume())
			collected_volumes.emplace_back(std::move(source));
	}

	volumes_collector.collect(container, collected_volumes, view_position);
}

void world::collect_meshes(mesh_container& container, const math::vector3d& render_origin)
{
	collected_model_instances.clear();
	for (const auto& object : objects->search_objects<model_object>())
	{
		if (model_instance_ptr source = object->get_model_instance())
			collected_model_instances.emplace_back(std::move(source));
	}
	effects->collect_model_instances(collected_model_instances);

	meshes_collector.collect(container, collected_model_instances, render_origin);
}
