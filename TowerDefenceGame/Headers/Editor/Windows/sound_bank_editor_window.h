#pragma once

#if defined(USE_IMGUI)
#include <string>
#include <string_view>
#include <vector>

#include "Editor/editor_items.h"
#include "Audio/sound_player.h"

//	サウンドバンク編集ウィンドウ
class sound_bank_editor_window : public editor_items::ibase_editor
{
public:
	explicit sound_bank_editor_window(std::string_view file_path);
	~sound_bank_editor_window() override;

	std::string get_label() const override;
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;
	bool is_dirty() const override { return dirty; }
	bool save() override;
	void on_file_moved(std::string_view new_file_path) override;

public:
	static void register_asset_handlers(std::shared_ptr<editor_items::context> context);

private:
	void scan_sound_files();
	bool load();

	void imgui_render_toolbar();
	void imgui_render_cue_list();
	void imgui_render_cue_detail(audio::sound_cue_desc& cue);

	//	音のファイルを一覧から選ぶ
	bool select_sound_file(const char* label, std::string& file_path);

private:
	std::string file_path;
	audio::sound_bank_desc bank;
	int selected_cue = -1;
	bool dirty = false;
	bool loaded = false;

	std::vector<std::string> sound_files;
	std::string message;
	audio::sound_instance preview;
};
#endif	//	defined(USE_IMGUI)
