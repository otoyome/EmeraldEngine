#pragma once

#include "Scenes/scene_base.h"
#include "Scenes/stationed_scene.h"

#include "object/gameobjects/ui/ui_image.h"
#include "object/gameobjects/ui/ui_text.h"

//	リザルトシーン
class minigame_result_scene : public scene_base
{
	SCENE_ADD_CHILDREN_DECRLEAR(minigame_result_scene, scene_base, stationed_scene);

public:
	enum class states
	{
		fade_in_wait,
		input_wait,
		fade_out_wait,
	};

	//	シーン切り替え前にminigame_sceneから結果を渡すためのデータ
	struct result_data
	{
		float clear_time = 0.0f;
		float max_speed_kmh = 0.0f;
		int trash_count = 0;
		int trash_max = 1;
	};

public:
	static void set_result(const result_data& result);

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

private:
	states state = states::fade_in_wait;
	static result_data result;
};
