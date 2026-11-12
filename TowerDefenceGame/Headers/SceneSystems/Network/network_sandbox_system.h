#pragma once

#include "Scenes/scene_system.h"

//	ネットワーク機能を試すためのサンドボックスシーン
class network_sandbox_system : public scene_system
{
	SCENE_SYSTEM_DECRLEAR(network_sandbox_system, scene_system)

public:
	network_sandbox_system();
	~network_sandbox_system() override;

	void on_begin() override;
	void pre_update(float elapsed_time) override;

#if defined(USE_IMGUI)
	void imgui_render() override;
#endif	//	defined(USE_IMGUI)
};