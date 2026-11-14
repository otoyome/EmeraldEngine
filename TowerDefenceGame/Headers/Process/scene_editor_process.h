#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <string>
#include <string_view>

#include "Process/framework_process.h"
#include "Scenes/scene_resource.h"

class scene_editor;
class editor_viewport;
struct play_session;

//	エディターの編集用のプロセス
class scene_editor_process : public framework_process
{
public:
	explicit scene_editor_process(framework& owner);
	~scene_editor_process() override;

	std::string_view get_key() const override { return "scene_editor"; }

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;
	void render(float elapsed_time) override;
	void submit_render_jobs(float elapsed_time) override;
	void post_render() override;
	void imgui_render() override;

private:
	//	アセットブラウザなどの開き先を登録
	void register_open_handlers();

	//	再生
	void start_play(const scene_resource_ptr& resource, std::string_view open_file);
	//	再生停止
	void stop_play();

	//	アプリを終了する
	void request_exit();

	//	起動時に開くシーン
	static std::string get_startup_scene_path();

private:
	std::unique_ptr<scene_editor> scene_edit;		//	シーンエディター
	std::unique_ptr<editor_viewport> viewport;		//	編集用のワールドを映すビューポート
	std::shared_ptr<play_session> session;			//	再生の状態
	bool owns_collision_filters = false;			//	当たり判定フィルターをこのプロセスで作ったか
	bool exit_requested = false;					//	Exit を選んだ
};
#endif	//	defined(USE_IMGUI)
