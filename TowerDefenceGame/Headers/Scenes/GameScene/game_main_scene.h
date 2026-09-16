#pragma once

#include "Scenes/scene_base.h"

#include "Scenes/StageScene/stage_00_scene.h"

#include "object/gameobjects/character/player/player_controller_object.h"
#include "object/gameobjects/ui/ui_object.h"

#include "effect_instance.h"

//	ゲームメインシーン
class game_main_scene : public scene_base
{
	//SCENE_DECRLEAR(game_main_scene, scene_base);
	SCENE_ADD_CHILDREN_DECRLEAR(game_main_scene, scene_base, stage_00_scene);

public:
	game_main_scene();
	game_main_scene(const game_main_scene&) = delete;
	game_main_scene(game_main_scene&&) = delete;
	~game_main_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
#ifdef USE_IMGUI
	virtual void imgui_render_core() override;
#endif

private:
	std::shared_ptr<player_controller_object> player_controller;
	std::shared_ptr<ui_object> test_ui;
	int hit_count = 0;

	std::shared_ptr<effect_instance> star_dust;
};
