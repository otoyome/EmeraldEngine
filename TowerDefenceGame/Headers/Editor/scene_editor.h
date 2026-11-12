#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Scenes/scene_resource.h"

class world;
class game_object;
class scene_system;

//	シーンエディター
class scene_editor
{
	//	編集中のシーンリソース
	struct edited_scene
	{
		scene_resource_ptr resource;
		std::vector<std::weak_ptr<game_object>> objects;
		std::unique_ptr<scene_system> system;
	};
	
public:
	//	編集中か
	bool is_editing() const { return !edited_scenes.empty(); }

	//	再生中か
	bool is_playing() const { return play_world != nullptr; }

	//	メインウィンドウに映すワールド
	world& get_active_world();
	world& get_edit_world() { return *edit_world; }
	world* get_play_world() { return play_world.get(); }

	//	編集中のシーンリソース取得
	scene_resource_ptr get_edit_resource() const;

public:
	scene_editor();
	~scene_editor();

	scene_editor(const scene_editor&) = delete;
	scene_editor& operator=(const scene_editor&) = delete;

public:
	//	編集リクエスト
	void request_edit(std::string_view file_path);

	//	再生リクエスト
	void request_play();
	void request_play(std::string_view file_path, std::string_view open_file = {});

	//	停止リクエスト
	void request_stop();

	//	リクエストの処理
	void apply_requests();

	//	保存
	bool save();

	//	メインメニューバーに並べる
	void imgui_menu();

	//	編集描画
	void imgui_render();

	//	サブシーンの追加・削除
	bool add_sub_scene(std::string_view file_path);
	void remove_sub_scene(const scene_resource_ptr& resource);

private:
	//	編集中のオブジェクトとシステムの設定をシーンリソースへ反映する
	bool apply_contents();

	bool open(std::string_view file_path);
	void close();

	//	@tool シーンリソースからオブジェクトとシステムを作り、編集用のワールドに登録する
	bool create_edited_scenes(const std::vector<scene_resource_ptr>& resources);
	void release_edited_scene(edited_scene& scene);

	void start_play(const scene_resource_ptr& resource);
	void stop_play();

	static std::unique_ptr<world> create_world();

private:
	std::unique_ptr<world> edit_world;			//	編集ワールド
	std::unique_ptr<world> play_world;			//	再生ワールド
	std::vector<edited_scene> edited_scenes;	//	編集中のシーン
	std::vector<resource::asset_handle<scene_resource>> sub_scenes;	//	サブシーン

	std::string edit_request;
	std::string play_file_request;
	std::string play_open_file_request;
	bool play_request = false;
	bool stop_request = false;
};
#endif	//	defined(USE_IMGUI)
