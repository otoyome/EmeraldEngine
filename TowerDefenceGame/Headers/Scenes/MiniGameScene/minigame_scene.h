#pragma once

#include "Scenes/scene_base.h"
#include "Scenes/stationed_scene.h"

#include "minigame_ui_scene.h"
#include "Scenes/StageScene/stage_00_scene.h"

#include "object/gameobjects/minigame/roomba_object.h"
#include "object/camera_object.h"
#include "effect_instance.h"

//	ゲームメインシーン
class minigame_scene : public scene_base
{
	//SCENE_DECRLEAR(minigame_scene, scene_base);
	SCENE_ADD_CHILDREN_DECRLEAR(minigame_scene, scene_base, stationed_scene, stage_00_scene, minigame_ui_scene);

public:
	enum class game_states
	{
		startup,
		start_count,
		main_game,
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
	std::shared_ptr<material> postprocess_material;

	std::shared_ptr<effect_instance> bubble_effect;
	std::shared_ptr<effect_instance> rain_effect;
	std::vector<UINT> bubble_emitter_ids, rain_emitter_ids;

	float game_timer;
	float countdown_timer;
	game_states game_state;
};
