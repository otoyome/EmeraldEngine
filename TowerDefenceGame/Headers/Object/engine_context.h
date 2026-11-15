#pragma once

class world;
class object_manager;
class scene_manager;
class effect_manager;
class debug_primitive_collector;
class game_object;

#include <functional>
#include <memory>
#include <string>
#include <string_view>

//	エンジンコンテキスト
class engine_context
{
public:
	explicit engine_context(world& owner);

	engine_context(const engine_context&) = delete;
	engine_context& operator=(const engine_context&) = delete;
	engine_context(engine_context&&) = delete;
	engine_context& operator=(engine_context&&) = delete;

public:
	world& get_world() const { return owner; }

	object_manager& get_objects() const;
	scene_manager& get_scenes() const;
	effect_manager& get_effects() const;

private:
	world& owner;

#if defined(USE_IMGUI)
public:		//	ImGui用
	//	シーンエディターの編集用のワールドか
	bool is_editing() const { return editing; }
	void set_editing(bool value) { editing = value; }

	debug_primitive_collector& get_debug_primitives() const;

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

	//	選んでいるオブジェクト
	std::shared_ptr<game_object> get_selected_object() const { return selected_object.lock(); }
	void set_selected_object(const std::shared_ptr<game_object>& object) { selected_object = object; }

	//	@tool アウトライナーで親子を付け替える時に任せる先（parent が空なら親から外す）。無ければそのまま付け替える
	using attach_handler = std::function<bool(const std::shared_ptr<game_object>& child, const std::shared_ptr<game_object>& parent)>;
	const attach_handler& get_attach_handler() const { return attach_object_handler; }
	void set_attach_handler(attach_handler value) { attach_object_handler = std::move(value); }

private:
	bool editing = false;
	bool embedded = false;
	bool asset_preview = false;
	bool viewport_hovered = false;
	math::float4 viewport_rect = { 0.0f, 0.0f, 0.0f, 0.0f };
	std::string window_id;
	std::string open_file;
	std::weak_ptr<game_object> selected_object;
	attach_handler attach_object_handler;
#endif	//	defined(USE_IMGUI)
};
