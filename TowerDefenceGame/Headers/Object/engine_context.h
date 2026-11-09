#pragma once

class world;
class object_manager;
class scene_manager;
class effect_manager;
class debug_primitive_collector;

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
	debug_primitive_collector& get_debug_primitives() const;

	//	窓に映しているワールドか
	bool is_embedded() const { return embedded; }
	void set_embedded(bool value) { embedded = value; }

	//	映している画像の上にカーソルがあるか
	bool is_viewport_hovered() const { return viewport_hovered; }
	void set_viewport_hovered(bool value) { viewport_hovered = value; }

	//	ImGui の窓の名前
	std::string make_window_name(std::string_view name) const { return std::string(name) + window_id; }
	void set_window_id(std::string value) { window_id = std::move(value); }

	//	シーンに開いてもらうファイル
	void set_open_file(std::string file_path) { open_file = std::move(file_path); }
	bool take_open_file(std::string& file_path) { if (open_file.empty()) return false; file_path = std::move(open_file); open_file.clear(); return true; }

private:
	bool embedded = false;
	bool viewport_hovered = false;
	std::string window_id;
	std::string open_file;
#endif	//	defined(USE_IMGUI)
};
