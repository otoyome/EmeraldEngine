#pragma once

#include "Scenes/scene_base.h"

#include "Audio/sound_player.h"

//	シーンをまたいで常駐させるシーン(フェード管理などを担う)
class stationed_scene : public scene_base
{
	SCENE_DECRLEAR(stationed_scene, scene_base);

public:
	stationed_scene();
	stationed_scene(const stationed_scene&) = delete;
	stationed_scene(stationed_scene&&) = delete;
	~stationed_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
#ifdef USE_IMGUI
	virtual void imgui_render_core() override;
#endif

private:
	audio::sound_bank_handle common_sound_bank;	//	共通サウンドバンク
};
