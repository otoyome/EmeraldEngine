#pragma once

#include "scenes/scene_base.h"

//	ネットワーク機能を試すためのサンドボックスシーン
class network_sandbox_scene : public scene_base
{
	SCENE_DECRLEAR(network_sandbox_scene, scene_base);

public:
	network_sandbox_scene();
	network_sandbox_scene(const network_sandbox_scene&) = delete;
	network_sandbox_scene(network_sandbox_scene&&) = delete;
	~network_sandbox_scene() override;

protected:
	bool initialize_core() override;
	void update_core(float elapsed_time) override;
#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)
};
