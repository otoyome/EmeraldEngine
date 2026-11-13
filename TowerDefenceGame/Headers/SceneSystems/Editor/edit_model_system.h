#pragma once

#include "Scenes/scene_system.h"

#include "Model/model_instance.h"

#include "object/model_object.h"
#include "object/camera_object.h"
#include "object/post_process_volume_object.h"

#if defined(USE_IMGUI)
#include "Model/data_converter.h"

#include "Animation/ik_solver.h"

#include "Editor/editor_items.h"

#include "Effect/effect_instance.h"

#include "Editor/editor_camera.h"

class edit_model_system : public scene_system
{
	SCENE_SYSTEM_DECRLEAR(edit_model_system, scene_system)

public:
	edit_model_system();
	edit_model_system(const edit_model_system&) = delete;
	edit_model_system(edit_model_system&&) = delete;
	~edit_model_system() override;

protected:
	void on_begin() override { initialize_core(); }
	void on_end() override { uninitialize_core(); }
	void pre_update(float elapsed_time) override { update_core(elapsed_time); }
	void imgui_render() override { imgui_render_core(); }
	bool has_panel(panel_regions region) const override;
	void imgui_render_panel(panel_regions region) override;

	bool initialize_core();
	bool uninitialize_core();
	void update_core(float elapsed_time);
	void imgui_render_core();

private:
	void save_model();
	void generate_lod();
	void generate_bounding_mesh_asset();
	void generate_cubic_sdf_asset();
	void limit_texture_resolution_asset();
	void generate_shadow_mesh_asset();
	void discard_lod();
	void discard_shadow_mesh();
	void optimize_mesh_asset();
	void generate_meshlet_asset();
	void strip_white_vertex_color_asset();

	void combine_meshes();
	void clear_combined_meshes();

private:	//	編集ウィンドウ纏め系
	bool initialize_edit_core();
	bool uninitialize_edit_core();
	void update_edit_core(float elapsed_time);
	void imgui_model_loader_window();
	void imgui_render_menu_bar();
	void imgui_render_mesh_list();
	void imgui_render_material_list();
	void imgui_render_material_detail();

private:	//	頂点カラー編集系
	bool initialize_vertex_color_edit();
	bool uninitialize_vertex_color_edit();
	void update_vertex_color_edit();
	void imgui_render_vertex_color_edit();
	void on_mouse_left_down_and_hold(void* data_ptr);
	void on_mouse_left_release(void* data_ptr);

private:	//	アニメーション編集系
	bool initialize_animation();
	bool uninitialize_animation();
	void update_animation();
	void imgui_render_animation();
	void imgui_render_animation_timeline();
	bool has_editable_animation() const;

	void imgui_render_morph();

	//	リターゲッティング系列
	void imgui_retarget_window();

	//	IK
	void imgui_render_ik();

private:
	void on_model_setup();
	void on_model_release();

	//	モデル全体が映る位置へエディターカメラを置く
	void focus_camera_on_model();

	//	編集用の複製マテリアルを取得(無ければ生成)
	material_resource_ptr get_working_material(const material_resource_ptr& original);

private:
	std::shared_ptr<camera_object> main_camera;
	std::shared_ptr<editor_camera_object> editor_camera;
	std::shared_ptr<post_process_volume_object> post_process_volume;

private:
	std::shared_ptr<model_object> edit_model;

	//	編集用の複製マテリアル
	struct material_copy
	{
		material_resource_ptr original;
		material_resource_ptr working;
	};
	std::vector<material_copy> material_copies;

	//	右に表示するマテリアルの番号
	std::size_t selected_material_index = 0;

	//	LOD生成レベル
	int generate_lod_level = 4;

	//	メッシュ結合のグリッド分割数（軸ごと）。{1,1,1} で全結合
	math::int3 combine_grid_divisions = { 1, 1, 1 };

	//	結合前の meshes / nodes 退避
	std::vector<model_resource::mesh> combine_backup_meshes;
	std::vector<model_resource::node> combine_backup_nodes;
	bool has_combine_backup = false;

	//	当たり判定メッシュ生成情報
	struct bounding_mesh_option
	{
		bool reverse_flag = true;
		bool use_lod = false;
		int lod_level = 0;

		//	当たり判定用にメッシュを統合する
		math::int3 combine_grid_divisions = { 1, 1, 1 };
	};
	bounding_mesh_option bounding_mesh_option;

	//	cubic_sdf 生成情報
	struct cubic_sdf_option
	{
		int resolution = 32;
		float bounds_padding_rate = 0.1f;
	};
	cubic_sdf_option cubic_sdf_option;

	//	テクスチャの最大解像度
	int texture_max_resolution = 2048;

	//	変換情報
	std::string load_model_path;
	struct convert_option_data
	{
		enum class kinds
		{
			none,
			mmd,
			gltf,
			emo,
		};
		kinds kind = kinds::none;
		data_converter::mmd::convert_model_data mmd;
		data_converter::gltf::convert_model_data gltf;
	} convert_option_data;

	std::shared_ptr<editor_items::context> editor_context;

private:	//	ボーン表示系
	bool display_bone = false;
	std::vector<std::string> selected_bones;
	std::vector<std::string> model_bone_names;

private:	//	メッシュ情報リスト
	struct edit_model_mesh_data
	{
		size_t node_index;
		bool visible = true;
		std::vector<model_instance::mesh*> meshes;
		std::vector<std::string> morphs;
	};
	std::vector<edit_model_mesh_data> edit_model_meshes;

private:
	//	リターゲッティング情報
	retarget_profile_data retarget_data;
	std::size_t selected_retarget_data_index = 0;

private:	//	頂点カラー編集系
	bool is_edit_vertex_color = false;
	std::shared_ptr<model_object> vertex_color_edit_model;
	std::shared_ptr<material_resource> vertex_color_edit_material;

	//	ブラシ情報
	struct brush_data
	{
		bool on = false;							//	処理
		math::float3 position;					//	座標
		float radius = 0.1f;						//	範囲
		math::float4 color{1, 1, 1, 0.5f};		//	色
		math::int4 color_flags{1, 1, 1, 1};	//	色適応フラグ
	};
	brush_data brush_data;

	//	左クリック
	uint64_t left_click_data_identifier[3] = { messenger::invalid_key, messenger::invalid_key, messenger::invalid_key };
};
#endif // defined(USE_IMGUI)
