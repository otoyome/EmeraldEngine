#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Process/framework_process.h"
#include "Scenes/scene_resource.h"
#include "Editor/editor_gizmo.h"
#include "Editor/editor_icons.h"
#include "Editor/place_objects_window.h"
#include "Editor/outliner_window.h"
#include "Editor/inspector_window.h"

class scene_editor;
class editor_viewport;
class game_process;

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
	void start_play(const std::vector<scene_resource_ptr>& resources, std::string_view open_file);
	//	再生停止
	void stop_play();
	//	再生の後始末
	void finish_play();
	//	再生中のゲームのプロセス
	game_process* find_game_process();

	//	アプリを終了する
	void request_exit();

	//	起動シーン
	static std::string get_startup_scene_path();
	//	前回シーン
	static std::vector<std::string> get_open_scene_paths();
	//	シーンを覚えておく
	void remember_open_scenes();

private:
	std::unique_ptr<scene_editor> scene_edit;		//	シーンエディター
	std::unique_ptr<editor_viewport> viewport;		//	ビューポート
	editor_gizmo gizmo;								//	ギズモ
	editor_icons icons;								//	アイコン
	place_objects_window place_window;				//	配置するオブジェクト一覧
	outliner_window outliner;						//	アウトライナー
	inspector_window inspector;						//	インスペクター
	bool playing = false;							//	ゲームプロセス中か？
	std::string play_open_file;						//	開くシーン
	bool exit_requested = false;					//	Exit を選んだか？
};
#endif	//	defined(USE_IMGUI)
