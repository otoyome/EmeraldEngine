#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "Editor/editor_items.h"
#include "Resource/asset_reference.h"
#include "Scenes/scene_resource.h"

//	シーンセット編集ウィンドウ
class scene_set_editor_window : public editor_items::ibase_editor
{
public:
	using open_scenes_function = std::function<void(std::string_view file_path)>;

public:
	scene_set_editor_window(std::string_view file_path, open_scenes_function open_scenes);
	~scene_set_editor_window() override;

	std::string get_label() const override;
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;
	bool is_dirty() const override { return dirty; }
	bool save() override;
	void on_file_moved(std::string_view new_file_path) override;

public:
	static void register_asset_handlers(std::shared_ptr<editor_items::context> context, open_scenes_function open_scenes);

private:
	bool load();

	void imgui_render_toolbar();
	void imgui_render_scene_list();
	void imgui_render_add_scene();

	bool add_scene(std::string_view scene_path);

private:
	static std::string resolve_path(const resource::asset_reference<scene_resource>& reference);

private:
	std::string file_path;
	open_scenes_function open_scenes;
	std::vector<resource::asset_reference<scene_resource>> scenes;
	bool dirty = false;
	bool loaded = false;
	std::string message;
};
#endif	//	defined(USE_IMGUI)
