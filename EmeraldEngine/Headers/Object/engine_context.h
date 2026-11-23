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

#if defined(USE_IMGUI)
#include "Object/editor_context.h"
#endif	//	defined(USE_IMGUI)

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

#if defined(USE_IMGUI)
public:		//	ImGui用
	editor_context& get_editor() const { return *editor; }
	bool is_editing() const { return editor->is_editing(); }
	debug_primitive_collector& get_debug_primitives() const;
#endif	//	defined(USE_IMGUI)

private:
	world& owner;

#if defined(USE_IMGUI)
	std::unique_ptr<editor_context> editor;
#endif	//	defined(USE_IMGUI)
};
