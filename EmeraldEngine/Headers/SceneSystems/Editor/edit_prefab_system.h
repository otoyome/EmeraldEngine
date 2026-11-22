#pragma once

#include "Scenes/scene_system.h"

#if defined(USE_IMGUI)
#include <memory>
#include <string>
#include <vector>

#include "Object/Graphics/camera_object.h"
#include "Object/prefab_resource.h"
#include "Editor/editor_camera.h"
#include "Editor/editor_gizmo.h"
#include "Editor/editor_icons.h"
#include "Editor/object_edit_operations.h"
#include "Editor/outliner_window.h"
#include "Editor/inspector_window.h"
#include "Editor/place_objects_window.h"

//	プレハブ編集シーン
class edit_prefab_system : public scene_system, public object_edit_operations
{
	SCENE_SYSTEM_DECRLEAR(edit_prefab_system, scene_system)

public:
	edit_prefab_system();
	edit_prefab_system(const edit_prefab_system&) = delete;
	edit_prefab_system(edit_prefab_system&&) = delete;
	~edit_prefab_system() override;

public:	//	object_edit_operations
	engine_context& get_edit_context() override { return get_context(); }
	std::shared_ptr<game_object> place_object(std::string_view type, const std::optional<math::vector3d>& position = std::nullopt) override;
	std::shared_ptr<game_object> place_object_as_child(std::string_view type, const std::shared_ptr<game_object>& parent) override;
	bool remove_object(const std::shared_ptr<game_object>& target) override;
	bool can_remove_object(const std::shared_ptr<game_object>& target) const override;
	bool attach_object(const std::shared_ptr<game_object>& child, const std::shared_ptr<game_object>& parent) override;

protected:
	void on_begin() override;
	void on_end() override;
	void pre_update(float elapsed_time) override;
	void imgui_render() override;

	bool has_panel(panel_regions region) const override;
	void imgui_render_panel(panel_regions region) override;
	bool imgui_render_viewport_overlay(const ImVec2& image_position, const ImVec2& image_size) override;
	bool request_close() override;

private:
	//	プレハブを開く・閉じる
	bool open_prefab(const std::string& file_path);
	void clear_objects();
	bool load_objects();

	//	保存・元に戻す
	bool save_prefab();
	void revert_prefab();

	//	今の中身を JSON にする
	std::string serialize_objects() const;
	bool is_dirty() const;

	//	オブジェクトを型から作ってプレハブに足す
	std::shared_ptr<game_object> add_object(std::string_view type, const math::vector3d& position);
	//	プレハブのオブジェクトか
	bool is_prefab_object(const std::shared_ptr<game_object>& object) const;
	//	target と空間の親子でつながったプレハブの子孫
	std::vector<std::shared_ptr<game_object>> collect_object_tree(const std::shared_ptr<game_object>& target) const;
	//	重ならない名前生成
	std::string make_unique_object_name(std::string_view base) const;

	void imgui_render_menu_bar();
	void imgui_render_close_popup();

private:
	std::shared_ptr<camera_object> main_camera;
	std::shared_ptr<editor_camera_object> editor_camera;

	prefab_resource_ptr prefab;	//	編集中プレハブ
	std::vector<std::shared_ptr<game_object>> objects;	//	プレハブの中身
	std::string saved_body;
	std::string message;

	bool open_close_popup = false;

	outliner_window outliner;
	inspector_window inspector;
	place_objects_window place_window;
	editor_gizmo gizmo;
	editor_icons icons;
};
#endif	//	defined(USE_IMGUI)
