#pragma once

#include <memory>
#include <vector>

#include "Graphics/camera.h"
#include "Graphics/light.h"
#include "Model/model_instance.h"
#include "Graphics/post_process_volume.h"
#include "Object/engine_context.h"

class object_manager;
class scene_manager;
class effect_manager;

//	ワールド
class world
{
public:
	world();
	~world();

	world(const world&) = delete;
	world& operator=(const world&) = delete;
	world(world&&) = delete;
	world& operator=(world&&) = delete;

public:
	object_manager& get_objects() { return *objects; }
	scene_manager& get_scenes() { return *scenes; }
	effect_manager& get_effects() { return *effects; }
	engine_context& get_context() { return context; }

public:	//	描画用の情報収集
	//	視点にするゲームカメラの検索
	camera_ptr find_view_camera();

	//	各種オブジェクト収集
	void collect_lights(light_container& container, const math::vector3d& render_origin, const light_view_info& view);
	void collect_post_process_volumes(post_process_volume_container& container, const math::vector3d& view_position);
	void collect_meshes(mesh_container& container, const math::vector3d& render_origin);

	const light_collector& get_light_collector() const { return lights_collector; }

private:
	engine_context context;

	std::unique_ptr<effect_manager> effects;
	std::unique_ptr<object_manager> objects;
	std::unique_ptr<scene_manager> scenes;

	light_collector lights_collector;
	mesh_collector meshes_collector;
	post_process_volume_collector volumes_collector;

	std::vector<light_ptr> collected_lights;
	std::vector<light_probe_ptr> collected_light_probes;
	std::vector<model_instance_ptr> collected_model_instances;
	std::vector<post_process_volume_ptr> collected_volumes;
};
