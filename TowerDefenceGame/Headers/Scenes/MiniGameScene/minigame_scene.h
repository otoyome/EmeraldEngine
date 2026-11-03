#pragma once

#include "Scenes/scene_base.h"
#include "Scenes/stationed_scene.h"

#include "minigame_ui_scene.h"
//#include "Scenes/StageScene/stage_00_scene.h"
#include "Scenes/MiniGameScene/minigame_stage_00_scene.h"
#include "Scenes/MiniGameScene/minigame_stage_01_scene.h"

#include "object/gameobjects/minigame/roomba_object.h"
#include "object/camera_object.h"
#include "effect_instance.h"

//	ゲームメインシーン
class minigame_scene : public scene_base
{
	SCENE_ADD_CHILDREN_DECRLEAR(minigame_scene, scene_base, stationed_scene, minigame_stage_01_scene, minigame_ui_scene);

public:
	enum class game_states
	{
		startup,
		warming_up,
		warming_up_end,
		countdown_setup,
		countdown,
		main_game,
		dead_stop_wait,
		dead_time_wait,
		result_start,
		result_wait,
		result_fade_out,
	};

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
	std::shared_ptr<material_resource> postprocess_material;

	effect_resource_ptr global_bubble_effect_resource;
	effect_resource_ptr bubble_effect_resource;
	effect_resource_ptr dust_effect_resource;
	std::vector<std::shared_ptr<effect_instance>> bubble_effect;
	std::vector<std::shared_ptr<effect_instance>> dust_effect;
	std::vector<UINT> bubble_emitter_ids;
	std::vector<UINT> dust_emitter_ids;

	float game_timer;
	float countdown_timer;
	game_states game_state;
	int trash_count;
	int trash_max;
	float max_speed_kmh;
};
