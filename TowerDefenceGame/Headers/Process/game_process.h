#pragma once

#include "Process/framework_process.h"

//	ゲームのプロセス
class game_process : public framework_process
{
public:
	using framework_process::framework_process;

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;
	void render(float elapsed_time) override;
#if defined(USE_IMGUI)
	void imgui_render() override;
#endif	//	defined(USE_IMGUI)
};
