#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Scenes/scene_resource.h"
#include "Editor/object_edit_operations.h"

class world;
struct world_settings;
class game_object;
class scene_system;

//	シーンエディター
class scene_editor : public object_edit_operations
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
	bool is_playing() const { return !playing_scenes.empty(); }

	world& get_edit_world() { return *edit_world; }
	engine_context& get_edit_context() override;

public:
	explicit scene_editor(const world_settings& settings);
	~scene_editor();

	scene_editor(const scene_editor&) = delete;
	scene_editor& operator=(const scene_editor&) = delete;

public:
	//	編集リクエスト
	void request_edit(std::string_view file_path);
	//	編集中のシーンに足すリクエスト
	void request_add(std::string_view file_path);

	//	再生リクエスト
	void request_play();
	void request_play(std::string_view file_path, std::string_view open_file = {});

	//	停止リクエスト
	void request_stop();

	//	オブジェクトを置く先のシーン
	const scene_resource_ptr& get_current_scene_resource() { return get_current_scene().resource; }
	//	オブジェクトを置く先のシーンを選ぶ
	void set_current_scene(const scene_resource_ptr& resource);

	//	編集中のシーン
	std::vector<scene_resource_ptr> get_edited_scenes() const;
	//	編集中のシーンに足せるシーンのパス
	std::vector<std::string> get_addable_scenes() const;
	//	編集中のシーンのオブジェクト
	std::vector<std::shared_ptr<game_object>> get_scene_objects(const scene_resource_ptr& resource) const;
	//	オブジェクトが入っている編集中のシーン
	scene_resource_ptr find_object_scene(const std::shared_ptr<game_object>& object);

	//	シーンリソースの編集窓で開く
	void open_edit_window(const scene_resource_ptr& resource) { edit_target = resource; edit_window_opened = true; }

	//	シーンの編集項目を描く
	void imgui_render_scene_settings(const scene_resource_ptr& resource);

	//	オブジェクトを型から作って置く
	std::shared_ptr<game_object> place_object(std::string_view type, const std::optional<math::vector3d>& position = std::nullopt) override;
	//	オブジェクトを型から作ってシーンの一番上に置く
	std::shared_ptr<game_object> place_object_in_scene(std::string_view type, const scene_resource_ptr& resource, const std::optional<math::vector3d>& position = std::nullopt);
	//	オブジェクトを型から作って parent の子にする
	std::shared_ptr<game_object> place_object_as_child(std::string_view type, const std::shared_ptr<game_object>& parent) override;
	//	プレハブの中身を複製して置く先のシーンに置く
	std::shared_ptr<game_object> place_prefab(std::string_view file_path, const std::optional<math::vector3d>& position = std::nullopt);

	//	再生の開始・停止を任せる先
	struct play_handlers
	{
		std::function<void(const std::vector<scene_resource_ptr>& resources, std::string_view open_file)> start;	//	再生を始める
		std::function<void()> stop;																					//	再生を止める
		std::function<void(std::string_view open_file)> open_file;													//	再生中のワールドにファイルを開いてもらう
	};
	void set_play_handlers(play_handlers value) { handlers = std::move(value); }

	//	再生が外で終わったことを知らせる
	void notify_play_finished() { playing_scenes.clear(); }

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

	//	編集中のシーンの追加・削除
	bool add_scene(std::string_view file_path);
	void remove_scene(const scene_resource_ptr& resource);
	bool can_remove_scene() const { return !is_playing() && edited_scenes.size() > 1; }

	//	オブジェクトを子毎削除する
	bool remove_object(const std::shared_ptr<game_object>& target) override;
	//	削除できるオブジェクトか
	bool can_remove_object(const std::shared_ptr<game_object>& target) const override;

	//	子供を親の子にする。親が別のシーンなら子孫ごと親のシーンへ移す
	bool attach_object(const std::shared_ptr<game_object>& child, const std::shared_ptr<game_object>& parent) override;
	//	オブジェクトを親から外し、子孫ごと別のシーンへ移す
	bool move_object_to_scene(const std::shared_ptr<game_object>& object, const scene_resource_ptr& resource);

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

	//	オブジェクトを型から作って編集用のワールドに登録
	std::shared_ptr<game_object> add_object(edited_scene& scene, std::string_view type, const math::vector3d& position);

	//	オブジェクトを置く先のシーン
	edited_scene& get_current_scene();
	//	編集用のワールドで重ならない名前生成
	std::string make_unique_object_name(std::string_view base) const;
	//	追加したオブジェクトを置く位置
	math::vector3d get_placement_position() const;

	//	オブジェクトが入っている編集中のシーン
	edited_scene* find_edited_scene(const std::shared_ptr<game_object>& object);
	//	targetと空間の親子でつながった子孫のオブジェクト
	std::vector<std::shared_ptr<game_object>> collect_object_tree(const std::shared_ptr<game_object>& target) const;
	//	targetと子孫を別のシーンへ移す
	void move_object_tree(const std::shared_ptr<game_object>& target, edited_scene& scene);

	//	シーンリソースの編集窓
	void imgui_render_edit_window();

	//	シーン開始処理
	void start_play(const std::vector<scene_resource_ptr>& resources, std::string_view open_file);

	//	シーン停止処理
	void stop_play();

	static std::unique_ptr<world> create_world(const world_settings& settings);

private:
	std::unique_ptr<world> edit_world;			//	編集ワールド
	std::vector<scene_resource_ptr> playing_scenes;	//	再生中のシーン
	play_handlers handlers;						//	再生の開始・停止を任せる先
	std::vector<edited_scene> edited_scenes;	//	編集中のシーン

	scene_resource_ptr edit_target;				//	編集窓に出すシーン
	bool edit_window_opened = false;
	scene_resource_ptr current_scene;			//	オブジェクトを置く先に選んだシーン

	std::string edit_request;
	std::vector<std::string> add_requests;
	std::string play_file_request;
	std::string play_open_file_request;
	bool play_request = false;
	bool stop_request = false;
};
#endif	//	defined(USE_IMGUI)
