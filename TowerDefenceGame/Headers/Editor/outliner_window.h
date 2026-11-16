#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Scenes/scene_resource.h"

class scene_editor;
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
	void render_transient_objects(world& target);
	//	置けるオブジェクトの型をメニューに並べる
	void render_type_menu(const std::function<void(const std::string& type)>& on_select);

	//	並べるシーンのオブジェクトを集める
	void collect_objects(const std::vector<scene_row>& scenes);
	//	オブジェクトの空間の節（spatial_object でなければ空）
	static const hierarchy* find_node(const std::shared_ptr<game_object>& object);
	//	子孫にオブジェクトを持つか
	bool has_object_children(const hierarchy* node) const;
	//	祖先にオブジェクトを持つか
	bool has_object_parent(const hierarchy* node) const;
	//	ドラッグで受け取ったオブジェクト
	std::shared_ptr<game_object> find_payload_object(const void* data) const;

private:
	scene_editor* editor = nullptr;		//	編集中のシーンを描いている間だけ持つ
	engine_context* context = nullptr;	//	描いている間だけ持つ
	std::unordered_map<const hierarchy*, std::shared_ptr<game_object>> owners;		//	空間の節を持つオブジェクト
	std::unordered_map<const game_object*, std::shared_ptr<game_object>> objects;	//	並べるシーンのオブジェクト
	std::function<void()> pending;													//	木を並べ終えてから行う操作
};
#endif	//	defined(USE_IMGUI)
