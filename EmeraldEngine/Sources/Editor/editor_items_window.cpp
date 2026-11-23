#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <algorithm>

#include "System/console_manager.h"
#include "Resource/resource_manager.h"
#include "imgui_internal.h"

void editor_items::register_tool_window(std::shared_ptr<context>& context, std::unique_ptr<ibase_editor> editor, console_variable<bool>& visible)
{
	auto& entry = context->tool_windows.emplace_back();
	entry.editor = std::move(editor);
	entry.visible = &visible;
}

editor_items::context::opened_window* editor_items::find_opened_window(const std::shared_ptr<context>& context, std::string_view key)
{
	const std::wstring path_key = resource_manager::make_path_key(key);
	for (auto& window : context->opened_windows)
	{
		if (resource_manager::make_path_key(window.key) == path_key)
			return &window;
	}
	return nullptr;
}

editor_items::ibase_editor* editor_items::find_window(const std::shared_ptr<context>& context, std::string_view key)
{
	context::opened_window* window = find_opened_window(context, key);
	return window && window->open ? window->editor.get() : nullptr;
}

void editor_items::focus_window(std::shared_ptr<context>& context, std::string_view key)
{
	if (context::opened_window* window = find_opened_window(context, key))
		window->focus = true;
}

void editor_items::add_window(std::shared_ptr<context>& context, std::string_view key, std::unique_ptr<ibase_editor> editor)
{
	auto& entry = context->opened_windows.emplace_back();
	entry.key = std::string(key);
	entry.editor = std::move(editor);
}

void editor_items::discard_windows(std::shared_ptr<context>& context, std::string_view path)
{
	std::string unused;
	for (auto& window : context->opened_windows)
	{
		if (resource_manager::replace_base_path(window.key, path, path, unused))
			window.open = false;
	}
}

void editor_items::move_windows(std::shared_ptr<context>& context, std::string_view old_path, std::string_view new_path)
{
	for (auto& window : context->opened_windows)
	{
		std::string moved_path;
		if (!resource_manager::replace_base_path(window.key, old_path, new_path, moved_path))
			continue;

		window.key = moved_path;
		window.editor->on_file_moved(moved_path);
	}
}

void editor_items::close_all_windows(std::shared_ptr<context>& context)
{
	for (auto& tool : context->tool_windows)
	{
		if (tool.was_visible)
			tool.editor->on_closed();
	}
	for (auto& window : context->opened_windows)
		window.editor->on_closed();

	context->opened_windows.clear();
	context->tool_windows.clear();
}

void editor_items::render_windows(std::shared_ptr<context>& context)
{
	render_window_menu_bar(context);

	for (auto& tool : context->tool_windows)
	{
		tool.editor->update();

		bool& visible = tool.visible->ref();
		if (visible)
			tool.editor->render_window(context, visible);
		if (tool.was_visible && !visible)
			tool.editor->on_closed();
		tool.was_visible = visible;
	}

	//	描画中に開かれたウィンドウは次のフレームから描く
	const std::size_t window_count = context->opened_windows.size();
	for (std::size_t index = 0; index < window_count; ++index)
	{
		if (!context->opened_windows[index].open)
			continue;

		ibase_editor* editor = context->opened_windows[index].editor.get();
		editor->update();
		if (context->opened_windows[index].focus)
		{
			ImGui::SetNextWindowFocus();
			context->opened_windows[index].focus = false;
		}

		bool open = true;
		editor->render_window(context, open);
		if (open)
			continue;

		//	未保存なら確認してから閉じる
		context::opened_window& closing = context->opened_windows[index];
		if (editor->is_dirty())
		{
			context->closing_window_key = closing.key;
			context->open_close_popup = true;
		}
		else
		{
			closing.open = false;
		}
	}

	render_window_close_popup(context);

	//	閉じたウィンドウを破棄する
	for (auto& window : context->opened_windows)
	{
		if (!window.open)
			window.editor->on_closed();
	}
	std::erase_if(context->opened_windows, [](const context::opened_window& window) { return !window.open; });
}

bool editor_items::begin_editor_menu_bar()
{
	ImGui::SetNextWindowSize(ImVec2(720.0f, 80.0f), ImGuiCond_FirstUseEver);
	const bool visible = ImGui::Begin("editor", &editor_window_open, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);
	if (visible && ImGui::BeginMenuBar())
		return true;
	ImGui::End();
	return false;
}

void editor_items::end_editor_menu_bar()
{
	ImGui::EndMenuBar();
	ImGui::End();
}

bool editor_items::consume_editor_close_request()
{
	const bool requested = !editor_window_open;
	editor_window_open = true;
	return requested;
}

void editor_items::editor_dock_space(const char* default_window)
{
	ImGui::SetNextWindowSize(ImVec2(720.0f, 80.0f), ImGuiCond_FirstUseEver);
	const bool visible = ImGui::Begin("editor", &editor_window_open, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);
	const ImGuiID dock_space_id = ImGui::GetID("editor_dock_space");

	//	配置がまだ無い時は、窓を広げて default_window を入れる
	if (!ImGui::DockBuilderGetNode(dock_space_id))
	{
		const ImVec2 dock_size(1280.0f, 720.0f);
		ImGui::SetWindowSize(ImVec2(dock_size.x, dock_size.y + ImGui::GetFrameHeight() * 2.0f + ImGui::GetStyle().WindowPadding.y * 2.0f));
		ImGui::DockBuilderAddNode(dock_space_id, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dock_space_id, dock_size);
		ImGui::DockBuilderDockWindow(default_window, dock_space_id);
		ImGui::DockBuilderFinish(dock_space_id);
	}

	//	窓が見えない時も、入っている窓が外れないように場所だけは残す
	ImGui::DockSpace(dock_space_id, ImVec2(0.0f, 0.0f), visible ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_KeepAliveOnly);
	ImGui::End();
}

void editor_items::render_window_menu_bar(std::shared_ptr<context>& context)
{
	if (!begin_editor_menu_bar())
		return;

	if (ImGui::BeginMenu("Window"))
	{
		for (auto& tool : context->tool_windows)
			ImGui::MenuItem(tool.editor->get_label().c_str(), nullptr, &tool.visible->ref());
		ImGui::EndMenu();
	}

	end_editor_menu_bar();
}

void editor_items::render_window_close_popup(std::shared_ptr<context>& context)
{
	constexpr const char* popup_name = "Unsaved Changes##editor_items";
	if (context->open_close_popup)
	{
		ImGui::OpenPopup(popup_name);
		context->open_close_popup = false;
	}

	if (!ImGui::BeginPopupModal(popup_name, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	context::opened_window* window = find_opened_window(context, context->closing_window_key);
	if (!window || !window->open)
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	ImGui::TextUnformatted(window->editor->get_label().c_str());
	ImGui::TextDisabled("%s", core::convert_shift_jis_to_utf8(window->key).c_str());
	ImGui::TextUnformatted("has unsaved changes.");
	ImGui::Separator();

	if (ImGui::Button("Save"))
	{
		if (window->editor->save())
		{
			window->open = false;
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Discard"))
	{
		window->open = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();
}
#endif	//	defined(USE_IMGUI)
