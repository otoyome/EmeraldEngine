#pragma once

#include <memory>

#include "Process/framework_process.h"

class console_command;

//	ゲームのプロセス
class game_process : public framework_process
{
public:
	explicit game_process(framework& owner);
	~game_process() override;

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;
	void render(float elapsed_time) override;
	void submit_render_jobs(float elapsed_time) override;
	void post_render() override;
#if defined(USE_IMGUI)
	void imgui_render() override;
#endif	//	defined(USE_IMGUI)

private:
	bool create_collision_filters();

#if defined(USE_IMGUI)
private:
	std::unique_ptr<console_command> pause_command;
	std::unique_ptr<console_command> step_command;
#endif	//	defined(USE_IMGUI)
};
