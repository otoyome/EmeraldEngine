#pragma once

#if defined(USE_IMGUI)
#include <string>
#include <string_view>
#include <vector>

#include "Editor/editor_items.h"
#include "Texture/texture_resource.h"

//	テクスチャリソース編集ウィンドウ
class texture_editor_window : public editor_items::ibase_editor
{
public:
	explicit texture_editor_window(std::string_view file_path);
	~texture_editor_window() override;

	//	アセットブラウザの開き先に登録する
	static void register_open_handler(std::shared_ptr<editor_items::context> context);

	std::string get_label() const override;
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;
	void on_file_moved(std::string_view new_file_path) override;

private:
	//	編集するテクスチャリソースを開く
	bool load();

	void imgui_render_preview();
	void imgui_render_settings();
	void imgui_render_sampler();
	void imgui_render_buttons();

	//	取り込み直しが要る変更か
	bool needs_reimport() const;

	void apply();
	void revert();

	void retire_current_texture();

private:
	std::string file_path;
	texture_resource_ptr target;
	texture_import_settings editing;
	std::string source_path_utf8;

	std::vector<texture_ptr> retired_textures;
};
#endif	//	defined(USE_IMGUI)
