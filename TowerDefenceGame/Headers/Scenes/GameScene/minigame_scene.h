#pragma once

#include "Scenes/scene_base.h"

#include "Scenes/StageScene/stage_00_scene.h"

#include "object/gameobjects/minigame/roomba_object.h"
#include "object/gameobjects/ui/ui_object.h"

#include "effect_instance.h"

#include "object/gameobjects/ui/ui_text.h"

//	ゲームメインシーン
class minigame_scene : public scene_base
{
	//SCENE_DECRLEAR(minigame_scene, scene_base);
	SCENE_ADD_CHILDREN_DECRLEAR(minigame_scene, scene_base, stage_00_scene);

public:
	minigame_scene();
	minigame_scene(const minigame_scene&) = delete;
	minigame_scene(minigame_scene&&) = delete;
	~minigame_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
#ifdef USE_IMGUI
	virtual void imgui_render_core() override;
#endif

private:
	std::shared_ptr<roomba_object> player_object;
	std::shared_ptr<ui_object> test_wet;
	std::shared_ptr<ui_object> test_heat;
	std::shared_ptr<ui_text> test_speed;

	std::shared_ptr<effect_instance> bubble_effect;
};
