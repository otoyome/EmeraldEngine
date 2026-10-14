#pragma once

#include "Scenes/scene_base.h"

#include "model_instance.h"

//	ステージ01シーン
class minigame_stage_01_scene : public scene_base
{
	SCENE_DECRLEAR(minigame_stage_01_scene, scene_base);

public:
	minigame_stage_01_scene();
	minigame_stage_01_scene(const minigame_stage_01_scene&) = delete;
	minigame_stage_01_scene(minigame_stage_01_scene&&) = delete;
	~minigame_stage_01_scene() override;

	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

public:
	void generate_model_collision(model_resource_ptr resource, const math::matrixf& transform);
	void generate_model_sdf(model_resource_ptr resource);

private:
	std::vector<uint32_t> sdf_ids;
};
