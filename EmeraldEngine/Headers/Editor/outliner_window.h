#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Scenes/scene_resource.h"

class scene_editor;
class object_edit_operations;
class scene_system;
class world;
class engine_context;
class game_object;
class hierarchy;

//	アウトライナー
class outliner_window
{
public:
	//	行をドラッグする時の受け渡し名
	static constexpr const char* payload_type = "OUTLINER_OBJECT";

	void imgui_render(scene_editor& editor);
	void imgui_render(world& target);

	//	プレハブエディターのパネル用
	void imgui_render_contents(object_edit_operations& operations, const std::vector<std::shared_ptr<game_object>>& objects);

private:
	//	並べるシーン
	struct scene_row
	{
		scene_resource_ptr resource;
		std::vector<std::shared_ptr<game_object>> objects;
		scene_system* system = nullptr;
	};

	//	窓を描く
	void render_window(world& target, const std::vector<scene_row>& scenes);
	//	窓の中身を描く
	void render_contents(engine_context& target, const std::vector<scene_row>& scenes, const std::vector<std::shared_ptr<game_object>>* loose_objects);
	//	シーンの行と、その下のオブジェクトを描く
	void render_scene(const scene_row& scene);
	//	シーンの右クリックのメニュー
	void render_scene_menu(const scene_row& scene);
	//	編集中のシーンに足せるシーンのメニュー
	void render_add_scene_menu();
	//	オブジェクトの行と、その子孫を描く
	void render_object(const std::shared_ptr<game_object>& object);
	//	子孫のうち最も近いオブジェクトを描く
	void render_children(const hierarchy* node);
	//	シーンに入っていない一時オブジェクトを描く
	void render_transient_objects(engine_context& target);
	//	置けるオブジェクトの型をメニューに並べる
	void render_type_menu(const std::function<void(const std::string& type)>& on_select);

	//	並べるシーンのオブジェクトを集める
	void collect_objects(const std::vector<scene_row>& scenes, const std::vector<std::shared_ptr<game_object>>* loose_objects);
	//	オブジェクトの空間の節（spatial_object でなければ空）
	static const hierarchy* find_node(const std::shared_ptr<game_object>& object);
	//	子孫にオブジェクトを持つか
	bool has_object_children(const hierarchy* node) const;
	//	祖先にオブジェクトを持つか
	bool has_object_parent(const hierarchy* node) const;
	//	ドラッグで受け取ったオブジェクト
	std::shared_ptr<game_object> find_payload_object(const void* data) const;

private:
	scene_editor* editor = nullptr;		//	編集シーン
	object_edit_operations* operations = nullptr;	//	オブジェクト編集操作機能
	engine_context* context = nullptr;	//	エンジンコンテキスト
	std::unordered_map<const hierarchy*, std::shared_ptr<game_object>> owners;		//	親オブジェクト
	std::unordered_map<const game_object*, std::shared_ptr<game_object>> objects;	//	並べるシーンのオブジェクト
	std::function<void()> pending;													//	木を並べ終えてから行う操作
};
#endif	//	defined(USE_IMGUI)
