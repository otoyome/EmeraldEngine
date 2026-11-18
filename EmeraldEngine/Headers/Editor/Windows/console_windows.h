#pragma once

#if defined(USE_IMGUI)
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"
#include "Editor/editor_items.h"
#include "System/console_manager.h"

//	コンソールウィンドウ
class console_window : public editor_items::ibase_editor
{
public:
	//	visible は console.toggle で切り替える表示状態
	explicit console_window(console_variable<bool>& visible);

	std::string get_label() const override { return "console"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;

private:
	//	入力中の語に合う候補を集める
	//	対象は先頭の語と、toggle / reset / help / list の引数
	void update_suggestions();

	//	候補で入力中の語を置き換える
	std::string apply_suggestion(const std::string& text, const std::string& suggestion) const;

	//	入力欄の補完と、候補の選択・履歴
	static int input_callback(ImGuiInputTextCallbackData* data);
	int handle_input_callback(ImGuiInputTextCallbackData* data);

	void imgui_render_suggestions(const ImVec2& input_position, float input_width);

private:
	//	コンソールウィンドウの状態
	console_variable<bool>& visible;
	bool focus_input = false;
	bool scroll_to_bottom = false;
	std::string input;	//	入力中の文字列
	std::vector<std::string> history;
	int history_index = -1;

	//	入力候補
	std::vector<std::string> suggestions;
	size_t suggestion_word_begin = 0;	//	候補で置き換える語の開始位置
	int suggestion_index = -1;			//	選択中の候補
	bool suggestions_hovered = false;

	console_command toggle_command;
};

//	コンソールコマンドウィンドウ
class console_commands_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "console commands"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;

private:
	//	コンソール要素リストウィンドウの状態
	std::string filter;
	bool changed_only = false;
	std::unordered_map<std::string, std::string> command_arguments;
};
#endif	//	defined(USE_IMGUI)
