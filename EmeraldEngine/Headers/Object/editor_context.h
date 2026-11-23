#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <string>
#include <string_view>

#include "Math/element.h"

class game_object;

//	エディター用のワールドの情報
class editor_context
{
public:
	//	シーンエディターの編集用のワールドか
	bool is_editing() const { return editing; }
	void set_editing(bool value) { editing = value; }

	//	窓に映しているワールドか
	bool is_embedded() const { return embedded; }
	void set_embedded(bool value) { embedded = value; }

	//	アセットの窓のワールドか
	bool is_asset_preview() const { return asset_preview; }
	void set_asset_preview(bool value) { asset_preview = value; }

	//	ImGui表示窓の上にカーソルがあるか
	bool is_viewport_hovered() const { return viewport_hovered; }
	void set_viewport_hovered(bool value) { viewport_hovered = value; }

	//	ImGui表示窓の位置と大きさ
	const math::float4& get_viewport_rect() const { return viewport_rect; }
	void set_viewport_rect(const math::float4& value) { viewport_rect = value; }

	//	ImGui の窓の名前
	std::string make_window_name(std::string_view name) const { return std::string(name) + window_id; }
	const std::string& get_window_id() const { return window_id; }
	void set_window_id(std::string value) { window_id = std::move(value); }

	//	シーンに開いてもらうファイル
	void set_open_file(std::string file_path) { open_file = std::move(file_path); }
	bool take_open_file(std::string& file_path) { if (open_file.empty()) return false; file_path = std::move(open_file); open_file.clear(); return true; }

	//	このワールドを映している窓を閉じてもらう
	void request_close_window() { close_window_requested = true; }
	bool take_close_window_request() { const bool result = close_window_requested; close_window_requested = false; return result; }

	//	選んでいるオブジェクト
	std::shared_ptr<game_object> get_selected_object() const { return selected_object.lock(); }
	void set_selected_object(const std::shared_ptr<game_object>& object) { selected_object = object; }

private:
	bool editing = false;
	bool embedded = false;
	bool asset_preview = false;
	bool viewport_hovered = false;
	math::float4 viewport_rect = { 0.0f, 0.0f, 0.0f, 0.0f };
	std::string window_id;
	std::string open_file;
	bool close_window_requested = false;
	std::weak_ptr<game_object> selected_object;
};
#endif	//	defined(USE_IMGUI)
