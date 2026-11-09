#pragma once

class world;
class object_manager;
class scene_manager;
class effect_manager;
class debug_primitive_collector;

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
	debug_primitive_collector& get_debug_primitives() const;
#endif	//	defined(USE_IMGUI)

private:
	world& owner;
};
