#pragma once

#include "Scenes/scene_base.h"

#include "object/gameobjects/minigame/roomba_hud_controller_object.h"

//	ゲームメインシーン
class minigame_ui_scene : public scene_base
{
	SCENE_DECRLEAR(minigame_ui_scene, scene_base);

public:
	minigame_ui_scene();
	minigame_ui_scene(const minigame_ui_scene&) = delete;
	minigame_ui_scene(minigame_ui_scene&&) = delete;
	~minigame_ui_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
#ifdef USE_IMGUI
	virtual void imgui_render_core() override;
#endif
};
