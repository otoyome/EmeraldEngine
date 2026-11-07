#pragma once

class world;
class object_manager;
class scene_manager;
class effect_manager;

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
};
