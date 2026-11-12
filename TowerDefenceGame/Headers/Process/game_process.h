#pragma once

#include <memory>

#include "Process/framework_process.h"
#include "Renderer/debug_view_settings.h"

class console_command;
class world;
class world_renderer;
class scene_editor;

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

	bool create_display_resources() override;
	void release_display_resources() override;

private:
	bool create_collision_filters();

	//	ワールド取得
	world* get_active_world();

private:
	std::unique_ptr<world> main_world;				//	ワールド
	std::unique_ptr<world_renderer> main_renderer;	//	ワールドレンダラー

#if defined(USE_IMGUI)
private:
	std::unique_ptr<scene_editor> scene_edit;		//	シーンエディター
	std::unique_ptr<console_command> pause_command;
	std::unique_ptr<console_command> step_command;
#endif	//	defined(USE_IMGUI)
};
