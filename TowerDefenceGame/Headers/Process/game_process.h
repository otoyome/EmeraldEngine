#pragma once

#include <memory>
#include <string>

#include "Process/framework_process.h"
#include "Renderer/debug_view_settings.h"
#include "Scenes/scene_resource.h"

class console_command;
class world;
class world_renderer;

#if defined(USE_IMGUI)
//	エディターから始めた再生の状態
struct play_session
{
	scene_resource_ptr scene;		//	再生するシーン
	std::string open_file;			//	再生中のワールドに開いてもらうファイル
	bool stop_requested = false;	//	停止の依頼
	bool finished = false;			//	ゲームのプロセスが終わった
};
#endif	//	defined(USE_IMGUI)

//	ゲームのプロセス
class game_process : public framework_process
{
public:
	explicit game_process(framework& owner);

#if defined(USE_IMGUI)
	//	エディターの再生用
	game_process(framework& owner, std::shared_ptr<play_session> session);
#endif	//	defined(USE_IMGUI)
	~game_process() override;

	bool is_closed() const override;
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
	bool owns_collision_filters = false;			//	当たり判定フィルターをこのプロセスで作ったか

#if defined(USE_IMGUI)
private:
	std::shared_ptr<play_session> session;		//	エディターの再生の状態（エディターの再生でなければ空）
	std::unique_ptr<console_command> pause_command;
	std::unique_ptr<console_command> step_command;
#endif	//	defined(USE_IMGUI)
};
