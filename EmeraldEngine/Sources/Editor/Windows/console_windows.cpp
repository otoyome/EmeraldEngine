#include "Editor/Windows/console_windows.h"

#if defined(USE_IMGUI)
#include "imgui.h"
#include "imgui_internal.h"

#include <functional>

namespace
{
	//	変数の値を型に合わせたウィジェットで編集する
	void edit_console_variable(console_variable_base& variable)
	{
		ImGui::BeginDisabled(variable.is_flag(console_flags::read_only));
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (auto value = dynamic_cast<console_variable<bool>*>(&variable))
		{
			ImGui::Checkbox("##value", &value->ref());
		}
		else if (auto value = dynamic_cast<console_variable<int>*>(&variable))
		{
			ImGui::DragInt("##value", &value->ref());
		}
		else if (auto value = dynamic_cast<console_variable<float>*>(&variable))
		{
			ImGui::DragFloat("##value", &value->ref(), 0.01f, 0.0f, 0.0f, "%.4f");
		}
		else if (auto value = dynamic_cast<console_variable<std::string>*>(&variable))
		{
			std::string utf8 = core::convert_shift_jis_to_utf8(value->get());
			if (ImGui::InputText("##value", utf8, ImGuiInputTextFlags_EnterReturnsTrue))
				value->set(core::convert_utf8_to_shift_jis(utf8));
		}
		else
		{
			ImGui::TextUnformatted(variable.to_string().c_str());
		}
		ImGui::EndDisabled();
	}
}

console_window::console_window(console_variable<bool>& visible)
	: visible(visible)
	, toggle_command("console.toggle", "コンソールウィンドウの表示切り替え",
		[this](const console_command::arguments&)
		{
			this->visible.ref() = !this->visible.get();
			focus_input = this->visible.get();

			//	「@」キーで開閉したときに入力された「@」を取り除く
			if (!input.empty() && input.back() == '@')
				input.pop_back();
		})
{
}

void console_window::update_suggestions()
{
	static constexpr size_t max_suggestions = 16;

	suggestions.clear();
	const std::string_view text(input);
	const size_t space = text.find(' ');
	const std::string_view first_word = text.substr(0, space);

	std::string_view word = text;
	suggestion_word_begin = 0;
	std::function<bool(const console_object&)> accept = [](const console_object&) { return true; };
	if (space != std::string_view::npos)
	{
		//	2語目だけを対象にする
		const std::string_view rest = text.substr(space + 1);
		if (rest.find(' ') != std::string_view::npos)
			return;
		if (first_word == "toggle")
			accept = [](const console_object& object) { return dynamic_cast<const console_variable<bool>*>(&object) != nullptr; };
		else if (first_word == "reset")
			accept = [](const console_object& object) { return object.is_variable(); };
		else if (first_word != "help" && first_word != "list")
			return;
		word = rest;
		suggestion_word_begin = space + 1;
	}
	else if (word.empty())
	{
		return;
	}

	std::vector<std::string> contains;
	for (auto* object : console_manager::instance().collect(""))
	{
		if (!accept(*object))
			continue;
		const std::string& name = object->get_name();
		if (name.starts_with(word))
			suggestions.emplace_back(name);
		else if (!word.empty() && name.find(word) != std::string::npos)
			contains.emplace_back(name);
	}
	suggestions.insert(suggestions.end(), contains.begin(), contains.end());
	if (suggestions.size() > max_suggestions)
		suggestions.resize(max_suggestions);

	//	入力した語と完全に一致する候補しかなければ表示しない
	if (suggestions.size() == 1 && suggestions.front() == word)
		suggestions.clear();
	if (suggestion_index >= static_cast<int>(suggestions.size()))
		suggestion_index = -1;
}

std::string console_window::apply_suggestion(const std::string& text, const std::string& suggestion) const
{
	return text.substr(0, std::min(suggestion_word_begin, text.size())) + suggestion + " ";
}

int console_window::input_callback(ImGuiInputTextCallbackData* data)
{
	return static_cast<console_window*>(data->UserData)->handle_input_callback(data);
}

int console_window::handle_input_callback(ImGuiInputTextCallbackData* data)
{
	auto replace_text = [data](const std::string& text)
	{
		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, text.c_str());
	};

	switch (data->EventFlag)
	{
		case ImGuiInputTextFlags_CallbackCompletion:
		{
			if (suggestions.empty())
				break;
			const std::string text(data->Buf, data->BufTextLen);

			//	選択中か候補が一つならそれで確定、複数なら共通する接頭辞まで埋める
			if (suggestion_index >= 0 || suggestions.size() == 1)
			{
				replace_text(apply_suggestion(text, suggestions[std::max(suggestion_index, 0)]));
			}
			else
			{
				std::string common = suggestions.front();
				for (auto& suggestion : suggestions)
				{
					size_t length = 0;
					while (length < common.size() && length < suggestion.size() && common[length] == suggestion[length])
						++length;
					common.resize(length);
				}
				const std::string word = text.substr(std::min(suggestion_word_begin, text.size()));
				if (common.size() > word.size() && common.starts_with(word))
					replace_text(text.substr(0, suggestion_word_begin) + common);
			}
			suggestion_index = -1;
			break;
		}
		case ImGuiInputTextFlags_CallbackHistory:
		{
			//	候補があるときは候補を選ぶ
			if (!suggestions.empty())
			{
				const int count = static_cast<int>(suggestions.size());
				if (data->EventKey == ImGuiKey_UpArrow)
					suggestion_index = std::min(suggestion_index + 1, count - 1);
				else if (data->EventKey == ImGuiKey_DownArrow)
					suggestion_index = std::max(suggestion_index - 1, -1);
				break;
			}

			if (history.empty())
				break;
			const int previous_index = history_index;
			const int history_count = static_cast<int>(history.size());
			if (data->EventKey == ImGuiKey_UpArrow)
			{
				if (history_index == -1)
					history_index = history_count - 1;
				else if (history_index > 0)
					--history_index;
			}
			else if (data->EventKey == ImGuiKey_DownArrow && history_index != -1)
			{
				if (++history_index >= history_count)
					history_index = -1;
			}
			if (previous_index != history_index)
				replace_text(history_index >= 0 ? history[history_index] : std::string());
			break;
		}
	}
	return 0;
}

void console_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	ImGui::SetNextWindowSize(ImVec2(720.0f, 320.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("console", &open))
	{
		auto& manager = console_manager::instance();

		//	出力
		const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("console_output", ImVec2(0.0f, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar))
		{
			const auto& output = manager.get_output();
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(output.size()));
			while (clipper.Step())
			{
				for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
				{
					const std::string utf8 = core::convert_shift_jis_to_utf8(output[index]);
					ImGui::TextUnformatted(utf8.c_str());
				}
			}
			if (scroll_to_bottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);
			scroll_to_bottom = false;
		}
		ImGui::EndChild();
		ImGui::Separator();

		//	入力
		update_suggestions();
		if (focus_input)
		{
			ImGui::SetKeyboardFocusHere();
			focus_input = false;
		}
		ImGui::SetNextItemWidth(-1.0f);
		const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
		if (ImGui::InputText("##console_input", input, flags, input_callback, this))
		{
			if (suggestion_index >= 0 && suggestion_index < static_cast<int>(suggestions.size()))
			{
				//	候補を選んでいる場合は確定だけ行う
				input = apply_suggestion(input, suggestions[suggestion_index]);
			}
			else
			{
				if (input.find_first_not_of(" \t") != std::string::npos)
				{
					if (history.empty() || history.back() != input)
						history.emplace_back(input);
					manager.execute(core::convert_utf8_to_shift_jis(input));
				}
				input.clear();
				history_index = -1;
				scroll_to_bottom = true;
			}
			suggestion_index = -1;
			focus_input = true;
		}
		const bool input_active = ImGui::IsItemActive();
		const ImVec2 input_position = ImGui::GetItemRectMin();
		const float input_width = ImGui::GetItemRectSize().x;

		//	予測変換の候補
		const bool show_suggestions = !suggestions.empty() && (input_active || suggestions_hovered);
		suggestions_hovered = false;
		if (show_suggestions)
			imgui_render_suggestions(input_position, input_width);
	}
	ImGui::End();
}

void console_window::imgui_render_suggestions(const ImVec2& input_position, float input_width)
{
	auto& manager = console_manager::instance();

	ImGui::SetNextWindowPos(input_position, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(input_width, 0.0f), ImVec2(FLT_MAX, 400.0f));
	const ImGuiWindowFlags suggestion_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
	if (ImGui::Begin("##console_suggestions", nullptr, suggestion_flags))
	{
		ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
		for (int index = static_cast<int>(suggestions.size()) - 1; index >= 0; --index)
		{
			const std::string& name = suggestions[index];
			const auto* object = manager.find(name);

			ImGui::PushID(index);
			if (ImGui::Selectable(name.c_str(), index == suggestion_index, ImGuiSelectableFlags_None, ImVec2(220.0f, 0.0f)))
			{
				input = apply_suggestion(input, name);
				suggestion_index = -1;
				focus_input = true;
			}
			if (object)
			{
				//	変数は現在値、コマンドは説明を添える
				ImGui::SameLine();
				const std::string detail = object->is_variable()
					? static_cast<const console_variable_base*>(object)->to_string()
					: core::convert_shift_jis_to_utf8(object->get_help());
				ImGui::TextDisabled("%s", detail.c_str());
			}
			ImGui::PopID();
		}
		suggestions_hovered = ImGui::IsWindowHovered();
	}
	ImGui::End();
}

void console_commands_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	ImGui::SetNextWindowSize(ImVec2(800.0f, 440.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("console commands", &open))
	{
		auto& manager = console_manager::instance();

		//	絞り込み
		ImGui::SetNextItemWidth(240.0f);
		ImGui::InputTextWithHint("##filter", "filter", filter);
		ImGui::SameLine();
		ImGui::Checkbox("changed only", &changed_only);
		ImGui::SameLine();
		if (ImGui::Button("console"))
			manager.execute("console.toggle");
		const std::string_view filter_text = filter;

		//	コマンドごとの割り当てキー
		std::unordered_map<std::string, std::string> bound_keys;
		for (auto& [key, command] : manager.get_bindings())
		{
			auto& keys = bound_keys[command];
			if (!keys.empty())
				keys += ", ";
			keys += key;
		}

		//	名前の先頭ごとにまとめて表示する
		std::string current_category;
		bool table_open = false;
		bool category_open = false;
		for (auto* listed : manager.collect(""))
		{
			auto object = manager.find(listed->get_name());
			if (!object)
				continue;

			const std::string& name = object->get_name();
			if (!filter_text.empty() && name.find(filter_text) == std::string::npos)
				continue;

			auto variable = object->is_variable() ? static_cast<console_variable_base*>(object) : nullptr;
			const bool changed = variable && variable->to_string() != variable->default_to_string();
			if (changed_only && !changed)
				continue;

			const size_t dot = name.find('.');
			const std::string category = dot != std::string::npos ? name.substr(0, dot) : std::string("(builtin)");
			if (category != current_category)
			{
				if (table_open)
					ImGui::EndTable();
				table_open = false;

				current_category = category;
				category_open = ImGui::CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
				if (category_open)
				{
					table_open = ImGui::BeginTable(category.c_str(), 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable);
					if (table_open)
					{
						ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed, 200.0f);
						ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 220.0f);
						ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed, 50.0f);
						ImGui::TableSetupColumn("help", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableHeadersRow();
					}
				}
			}
			if (!table_open)
				continue;

			ImGui::TableNextRow();
			ImGui::PushID(name.c_str());

			//	名前
			ImGui::TableSetColumnIndex(0);
			const std::string label = name + (object->is_flag(console_flags::persist) ? " *" : "");
			if (changed)
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", label.c_str());
			else
				ImGui::TextUnformatted(label.c_str());

			//	値の編集、またはコマンドの実行
			ImGui::TableSetColumnIndex(1);
			if (variable)
			{
				if (changed)
				{
					if (ImGui::SmallButton("reset"))
						variable->reset();
					ImGui::SameLine();
				}
				edit_console_variable(*variable);
			}
			else
			{
				auto& arguments = command_arguments[name];
				const bool run = ImGui::SmallButton("run");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-FLT_MIN);
				const bool entered = ImGui::InputTextWithHint("##args", "args", arguments, ImGuiInputTextFlags_EnterReturnsTrue);
				if (run || entered)
					manager.execute(name + " " + core::convert_utf8_to_shift_jis(arguments));
			}

			//	割り当てキー
			ImGui::TableSetColumnIndex(2);
			if (auto it = bound_keys.find(name); it != bound_keys.end())
				ImGui::TextUnformatted(it->second.c_str());

			//	説明
			ImGui::TableSetColumnIndex(3);
			const std::string help = core::convert_shift_jis_to_utf8(object->get_help());
			ImGui::TextWrapped("%s", help.c_str());

			ImGui::PopID();
		}
		if (table_open)
			ImGui::EndTable();
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)
