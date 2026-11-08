#pragma once

#if defined(USE_IMGUI)
#include <string>
#include <string_view>
#include <vector>

#include "Texture/texture_resource.h"

//	テクスチャリソースの編集画面
class texture_editor
{
	SINGLETON(texture_editor)

public:
	//	アセットブラウザの開き先の名前
	static constexpr std::string_view open_handler_name = "texture_editor";

public:
	texture_editor();
	~texture_editor();

	//	アセットブラウザの開き先に登録する
	void register_open_handler();

	//	編集するテクスチャリソースを開く
	bool open(std::string_view file_path);

	void imgui_render();

private:
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
	texture_resource_ptr target;
	texture_import_settings editing;
	std::string source_path_utf8;
	bool visible = false;
	bool focus = false;

	std::vector<texture_ptr> retired_textures;
};
#endif	//	defined(USE_IMGUI)
