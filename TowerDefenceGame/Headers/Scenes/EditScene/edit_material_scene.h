#pragma once

#include "scenes/scene_base.h"

#if defined(USE_IMGUI)
#include "Model/model_resource.h"
#include "Model/model_resource_utility.h"
#include "Effect/effect_resource.h"
#include "Effect/effect_instance.h"

#include "object/model_object.h"
#include "object/camera_object.h"
#include "object/post_process_volume_object.h"
#include "Editor/editor_items.h"
#include "Editor/editor_camera.h"

//	マテリアル編集シーン
class edit_material_scene : public scene_base
{
	SCENE_DECRLEAR(edit_material_scene, scene_base);

public:
	edit_material_scene();
	edit_material_scene(const edit_material_scene&) = delete;
	edit_material_scene(edit_material_scene&&) = delete;
	~edit_material_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
	void imgui_render_core() override;

private:
	//	編集対象のマテリアル
	struct material_entry
	{
		std::string label;
		material_resource_ptr material;
	};

private:
	void open_source(const std::string& file_path, bool force_load);
	void save_source();
	void revert_source();
	void collect_materials();
	material_resource_ptr get_selected_material() const;

	//	開いているリソースの今のパス
	std::string get_source_file_path() const;

	void scan_preview_models();
	void change_preview_model(const std::string& file_path);
	void change_preview_shape(model_resource_utility::primitive_shapes shape);
	void set_preview_resource(const model_resource_ptr& resource, double lift);
	std::string get_preview_label() const;
	void apply_preview_materials();
	void restart_preview_effect();
	void clear_preview_effect();

	void imgui_render_source();
	void imgui_render_preview();
	void imgui_render_material_list();
	void imgui_render_material_detail();

private:
	std::shared_ptr<camera_object> main_camera;
	std::shared_ptr<editor_camera_object> editor_camera;
	std::shared_ptr<editor_items::context> editor_context;

private:
	model_resource_ptr source_model;
	effect_resource_ptr source_effect;
	material_resource_ptr source_material;

	std::vector<material_entry> materials;
	int selected_material = -1;
	bool dirty = false;
	std::string message;

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

	effect_instance_ptr preview_effect;
	bool loop_preview_effect = true;
};
#endif // defined(USE_IMGUI)
