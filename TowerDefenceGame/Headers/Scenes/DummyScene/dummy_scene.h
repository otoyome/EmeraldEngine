#pragma once

#include "scenes/scene_base.h"

class dummy_scene : public scene_base
{
	SCENE_DECRLEAR(dummy_scene, scene_base);

public:
	dummy_scene();
	dummy_scene(const dummy_scene&) = delete;
	dummy_scene(dummy_scene&&) = delete;
	~dummy_scene() override;

protected:
	bool initialize_core() override;
	void update_core(float elapsed_time) override;
	void imgui_render_core() override;
};
