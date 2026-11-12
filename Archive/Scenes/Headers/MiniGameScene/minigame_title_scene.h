#pragma once

#include "Scenes/scene_base.h"
#include "Scenes/stationed_scene.h"

#include "object/gameobjects/ui/ui_image.h"

#include "Scenes/MiniGameScene/minigame_title_sound.h"
#include "Input/input_action.h"

//	タイトルシーン
class minigame_title_scene : public scene_base
{
	SCENE_ADD_CHILDREN_DECRLEAR(minigame_title_scene, scene_base, stationed_scene);

public:
	enum class states
	{
		fade_in_wait,
		input_wait,
		fade_out_wait,
	};

public:
	minigame_title_scene();
	minigame_title_scene(const minigame_title_scene&) = delete;
	minigame_title_scene(minigame_title_scene&&) = delete;
	~minigame_title_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
#ifdef USE_IMGUI
	virtual void imgui_render_core() override;
#endif

private:
	states state = states::fade_in_wait;
	input::action decide_action;
	signals::connection_list connections;

	minigame_title_sound sound;		//	購読を持つので最後に置く
};
