#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Process/framework_process.h"
#include "Renderer/debug_view_settings.h"
#include "Scenes/scene_resource.h"

#if defined(USE_IMGUI)
#include "Editor/outliner_window.h"
#include "Editor/inspector_window.h"
#endif	//	defined(USE_IMGUI)

class console_command;
class world;
class world_renderer;

//	ゲームのプロセス
class game_process : public framework_process
{
public:
	//	プロセスキー
	static constexpr std::string_view process_key = "game";

public:
	//	キー取得
	std::string_view get_key() const override { return process_key; }

	//	ワールド取得
	world* get_world() { return main_world.get(); }

public:
	explicit game_process(framework& owner);
	game_process(framework& owner, std::vector<scene_resource_ptr> start_scenes);
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
	std::unique_ptr<world> main_world;				//	ワールド
	std::unique_ptr<world_renderer> main_renderer;	//	ワールドレンダラー
	std::vector<scene_resource_ptr> start_scenes;	//	起動シーン

#if defined(USE_IMGUI)
private:
	std::unique_ptr<console_command> pause_command;
	std::unique_ptr<console_command> step_command;
	outliner_window outliner;					//	シーンとオブジェクトの木の窓
	inspector_window inspector;					//	選んでいるオブジェクトの設定の窓
#endif	//	defined(USE_IMGUI)
};
