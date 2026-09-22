#pragma once

#include "Scenes/scene_base.h"

#include "object/gameobjects/ui/ui_image.h"

//	リザルトシーン
class minigame_result_scene : public scene_base
{
	SCENE_DECRLEAR(minigame_result_scene, scene_base);

public:
	minigame_result_scene();
	minigame_result_scene(const minigame_result_scene&) = delete;
	minigame_result_scene(minigame_result_scene&&) = delete;
	~minigame_result_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
#ifdef USE_IMGUI
	virtual void imgui_render_core() override;
#endif
};
