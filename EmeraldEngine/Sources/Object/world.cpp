#include "Object/world.h"
#include "Object/object_manager.h"
#include "Object/Graphics/camera_object.h"
#include "Object/Graphics/light_object.h"
#include "Object/Graphics/light_probe_object.h"
#include "Object/Graphics/model_object.h"
#include "Object/Graphics/post_process_volume_object.h"
#include "Scenes/scene_manager.h"
#include "Effect/effect_instance.h"
#include "Graphics/Debug/debug_primitive_collector.h"

world::world()
: context(*this)
, effects(std::make_unique<effect_manager>())
, objects(std::make_unique<object_manager>(context))
, scenes(std::make_unique<scene_manager>(context))
#if defined(USE_IMGUI)
, debug_primitives(std::make_unique<debug_primitive_collector>())
#endif	//	defined(USE_IMGUI)
{
}

world::~world()
{
	scenes->remove_all_scene();
	scenes.reset();

	objects->finalize();
	objects.reset();

	effects.reset();
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
		if (!object->is_active())
			continue;
		if (light_ptr source = object->get_light())
			collected_lights.emplace_back(std::move(source));
	}

	collected_light_probes.clear();
	for (const auto& object : objects->search_objects<light_probe_object>())
	{
		if (!object->is_active())
			continue;
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
