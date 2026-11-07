#pragma once

#include "scenes/scene_base.h"

#if defined(USE_IMGUI)
#include "effect_resource.h"
#include "effect_instance.h"

#include "object/camera_object.h"
#include "object/post_process_volume_object.h"
#include "Editor/editor_camera.h"

class edit_particle_scene : public scene_base
{
	SCENE_DECRLEAR(edit_particle_scene, scene_base);

public:
	edit_particle_scene();
	edit_particle_scene(const edit_particle_scene&) = delete;
	edit_particle_scene(edit_particle_scene&&) = delete;
	~edit_particle_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
	void imgui_render_core() override;

private:
	void on_new();
	void generate_default_effect();
	void open_effect(const std::string& file_path);

private:
	std::shared_ptr<camera_object> main_camera;
	std::shared_ptr<post_process_volume_object> post_process_volume;
	std::shared_ptr<editor_camera_object> editor_camera;

private:
	std::shared_ptr<editor_items::context> editor_context;

	effect_resource_ptr edit_resource;
	effect_instance_ptr edit_instance;

private:
	//	エディター上で読み込み・登録しているcubic_sdf
	struct edit_sdf_data
	{
		cache_string file_path;
		uint32_t sdf_id = 0xFFFFFFFF;
		math::float3 bounds_min = { 0, 0, 0 };
		math::float3 bounds_max = { 0, 0, 0 };
		uint32_t resolution = 0;
		bool show_voxels = true;

		std::vector<math::float4> voxel_points;	//	ボクセル可視化用
	};
	std::vector<edit_sdf_data> edit_sdfs;

	void imgui_render_sdf();
	void draw_sdf_debug_primitives();
};
#endif // defined(USE_IMGUI)
