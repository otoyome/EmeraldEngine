#pragma once

#include "Scenes/scene_system.h"

#if defined(USE_IMGUI)
#include "Model/model_resource.h"
#include "Model/model_resource_utility.h"
#include "Effect/effect_resource.h"
#include "Effect/effect_instance.h"

#include "Object/Graphics/model_object.h"
#include "Object/Graphics/camera_object.h"
#include "Object/Graphics/post_process_volume_object.h"
#include "Object/UI/ui_image.h"
#include "Editor/editor_items.h"
#include "Editor/editor_camera.h"
#include "Editor/editor_transaction.h"
#include "Editor/editor_edit_tracker.h"

//	マテリアル編集シーン
class edit_material_system : public scene_system
{
	SCENE_SYSTEM_DECRLEAR(edit_material_system, scene_system)

public:
	edit_material_system();
	edit_material_system(const edit_material_system&) = delete;
	edit_material_system(edit_material_system&&) = delete;
	~edit_material_system() override;

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
	//	編集対象のマテリアル
	struct material_entry
	{
		std::string label;
		material_resource_ptr material;	//	編集用の複製
		material_resource_ptr original;	//	元のマテリアル
	};

private:
	void open_source(const std::string& file_path, bool force_load);
	void save_source();
	void revert_source();
	void collect_materials();

	//	選択中のマテリアル取得
	material_resource_ptr get_selected_material() const;

	//	元のマテリアルに対応する編集用の複製
	material_resource_ptr find_working_material(const material_resource_ptr& original) const;

	//	開いているリソースの今のパス
	std::string get_source_file_path() const;

	void scan_preview_models();
	void change_preview_model(const std::string& file_path);
	void change_preview_shape(model_resource_utility::primitive_shapes shape);
	void set_preview_resource(const model_resource_ptr& resource, double lift);
	std::string get_preview_label() const;
	void apply_preview_materials();
	void make_preview_effect_resource();
	void restart_preview_effect();
	void clear_preview_effect();

	//	サーフェス以外のマテリアルを確かめる仮の表示
	void update_domain_preview();
	void make_domain_preview(const material_resource_ptr& material);
	void clear_domain_preview();
	effect_resource_ptr make_domain_preview_effect(const material_resource_ptr& material) const;
	void attach_preview_post_process(const material_resource_ptr& material);

	//	元に戻す・やり直す
	void redo();
	void undo();

	void imgui_render_menu_bar();
	void imgui_render_preview();
	void imgui_render_material_list();
	void imgui_render_material_detail();

private:
	std::shared_ptr<camera_object> main_camera;
	std::shared_ptr<editor_camera_object> editor_camera;
	std::shared_ptr<editor_items::context> items_context;

private:
	model_resource_ptr source_model;
	effect_resource_ptr source_effect;
	material_resource_ptr source_material;

	std::vector<material_entry> materials;
	int selected_material = -1;
	bool dirty = false;
	std::string message;

	editor_transaction_history history;		//	元に戻す・やり直すの履歴
	editor_change_tracker change_tracker;	//	選んでいるマテリアルの変更の追跡

private:
	enum class preview_kinds
	{
		shape,
		file,
	};
	preview_kinds preview_kind = preview_kinds::shape;
	model_resource_utility::primitive_shapes preview_shape = model_resource_utility::primitive_shapes::sphere;
	std::array<model_resource_ptr, magic_enum::enum_count<model_resource_utility::primitive_shapes>()> shape_resources;

	model_resource_ptr floor_resource;
	std::shared_ptr<model_object> floor_model;
	bool show_floor = true;

	std::vector<std::string> preview_model_files;
	std::string preview_model_path;
	std::shared_ptr<model_object> preview_model;
	bool override_preview_slots = true;

	effect_resource_ptr preview_effect_resource;
	effect_instance_ptr preview_effect;
	bool loop_preview_effect = true;

	//	仮の表示に使っているマテリアルと、表示しているもの
	material_resource_ptr domain_preview_material;
	effect_instance_ptr domain_preview_effect;
	std::shared_ptr<ui_image> preview_ui;
	std::shared_ptr<post_process_volume_object> preview_volume;			//	マテリアルを足したボリューム
	std::shared_ptr<post_process_volume_object> own_preview_volume;		//	シーンにボリュームが無い時に作った仮のボリューム
};
#endif // defined(USE_IMGUI)
