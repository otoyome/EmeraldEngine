#pragma once

#include "Scenes/scene_system.h"

#if defined(USE_IMGUI)
#include "Effect/effect_resource.h"
#include "Effect/effect_instance.h"

#include "Object/Graphics/camera_object.h"
#include "Object/Graphics/post_process_volume_object.h"
#include "Editor/editor_camera.h"

class edit_particle_system : public scene_system
{
	SCENE_SYSTEM_DECRLEAR(edit_particle_system, scene_system)

public:
	edit_particle_system();
	edit_particle_system(const edit_particle_system&) = delete;
	edit_particle_system(edit_particle_system&&) = delete;
	~edit_particle_system() override;

protected:
	void on_begin() override { initialize_core(); }
	void on_end() override { uninitialize_core(); }
	void pre_update(float elapsed_time) override { update_core(elapsed_time); }
	bool has_panel(panel_regions region) const override;
	void imgui_render_panel(panel_regions region) override;

	bool initialize_core();
	bool uninitialize_core();
	void update_core(float elapsed_time);

private:
	void on_new();
	void rebuild_edit_instance();
	void imgui_render_menu_bar();
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
	size_t selected_node_index = 0;	//	詳細を出すノードの番号

private:
	//	エディター上で読み込み・登録しているcubic_sdf
	struct edit_sdf_data
	{
		core::cache_string file_path;
		uint32_t sdf_id = 0xFFFFFFFF;
		math::float3 bounds_min = { 0, 0, 0 };
		math::float3 bounds_max = { 0, 0, 0 };
		uint32_t resolution = 0;
		bool show_voxels = true;

		std::vector<math::float4> voxel_points;	//	ボクセル可視化用
	};
	std::vector<edit_sdf_data> edit_sdfs;

	void load_sdf_files();
	void imgui_render_sdf();
	void draw_sdf_debug_primitives();
};
#endif // defined(USE_IMGUI)
