#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <memory>
#include <optional>
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
	bool is_playing() const { return playing_scene != nullptr; }

	world& get_edit_world() { return *edit_world; }

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

	//	@tool オブジェクトを置く先のシーン（scenes 窓で選んだシーン。選んでいなければ開いているシーン）
	const scene_resource_ptr& get_current_scene_resource() { return get_current_scene().resource; }

	//	@tool オブジェクトを型から作って、開いているシーン（親のシーン）に置く（position が無ければエディターカメラの前方。作った物を選ぶ）
	std::shared_ptr<game_object> place_object(std::string_view type, const std::optional<math::vector3d>& position = std::nullopt);

	//	再生の開始・停止を任せる先
	struct play_handlers
	{
		std::function<void(const scene_resource_ptr& resource, std::string_view open_file)> start;	//	再生を始める
		std::function<void()> stop;																	//	再生を止める
		std::function<void(std::string_view open_file)> open_file;									//	再生中のワールドにファイルを開いてもらう
	};
	void set_play_handlers(play_handlers value) { handlers = std::move(value); }

	//	再生が外で終わった（ゲームのプロセスが自分で終わった）ことを知らせる
	void notify_play_finished() { playing_scene.reset(); }

	//	リクエストの処理
	void apply_requests();

	//	保存
	bool save();

	//	メインメニューバーに並べる
	void imgui_menu();

	//	編集描画
	void imgui_render();

	//	編集中のみの更新
	void editor_update(float elapsed_time);

	//	サブシーンの追加・削除
	bool add_sub_scene(std::string_view file_path);
	void remove_sub_scene(const scene_resource_ptr& resource);

	//	@tool オブジェクトを子孫ごと削除する（編集中のシーンのオブジェクトだけ。一時オブジェクトは消さない）
	bool remove_object(const std::shared_ptr<game_object>& target);
	//	@tool 削除できるオブジェクトか
	bool can_remove_object(const std::shared_ptr<game_object>& target) const;

	//	@tool child を parent の子にする（parent が空なら親から外す）。親が別のシーンなら子孫ごと親のシーンへ移す
	bool attach_object(const std::shared_ptr<game_object>& child, const std::shared_ptr<game_object>& parent);

private:
	//	編集中のオブジェクトとシステムの設定をシーンリソースへ反映する
	bool apply_contents();

	//	シーンオープン/クローズ
	bool open(std::string_view file_path);
	void close();

	//	編集シーン生成
	bool create_edited_scenes(const std::vector<scene_resource_ptr>& resources);

	//	編集シーン破棄
	void release_edited_scene(edited_scene& scene);

	//	シーンのシステムを付け替え
	void set_system(edited_scene& scene, std::string_view type);

	//	@tool オブジェクトを型から作って編集用のワールドに登録し、scene のオブジェクトにする（作った物を選ぶ）
	std::shared_ptr<game_object> add_object(edited_scene& scene, std::string_view type, const math::vector3d& position);

	//	@tool オブジェクトを置く先のシーン（scenes 窓で選んだシーン。選んでいなければ開いているシーン）
	edited_scene& get_current_scene();
	//	@tool 編集用のワールドで重ならない名前（重なれば後ろに _番号 を付ける）
	std::string make_unique_object_name(std::string_view base) const;
	//	@tool 追加したオブジェクトを置く位置（エディターカメラの前方）
	math::vector3d get_placement_position() const;

	//	@tool オブジェクトが入っている編集中のシーン（無ければ空）
	edited_scene* find_edited_scene(const std::shared_ptr<game_object>& object);
	//	@tool target と、空間の親子でつながった子孫のオブジェクト（一時オブジェクトは除く）
	std::vector<std::shared_ptr<game_object>> collect_object_tree(const std::shared_ptr<game_object>& target) const;

	//	シーンリソースの編集窓
	void imgui_render_edit_window();

	//	シーン開始処理
	void start_play(const scene_resource_ptr& resource, std::string_view open_file);

	//	シーン停止処理
	void stop_play();

	static std::unique_ptr<world> create_world();

private:
	std::unique_ptr<world> edit_world;			//	編集ワールド
	scene_resource_ptr playing_scene;			//	再生中のシーン
	play_handlers handlers;						//	再生の開始・停止を任せる先
	std::vector<edited_scene> edited_scenes;	//	編集中のシーン
	std::vector<resource::asset_handle<scene_resource>> sub_scenes;	//	サブシーン
	scene_resource_ptr edit_target;				//	編集窓に出すシーン
	bool edit_window_opened = false;
	scene_resource_ptr current_scene;			//	オブジェクトを置く先に選んだシーン

	std::string edit_request;
	std::string play_file_request;
	std::string play_open_file_request;
	bool play_request = false;
	bool stop_request = false;
};
#endif	//	defined(USE_IMGUI)
