#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "System/signals.h"
#include "Scenes/scene_resource.h"

class engine_context;
class scene_instance;

//	シーン管理クラス
class scene_manager
{
	//	追加シーンデータ
	struct add_scene_data
	{
		std::string name;
		scene_resource_ptr resource;
	};

public:
	//	存在しているシーン名リスト取得
	std::vector<std::string> get_valid_scene_names() const;

	//	シーンリスト取得
	const std::vector<std::pair<std::string, std::unique_ptr<scene_instance>>>& get_scenes() const { return scenes; }

	//	シーン全体で共有する通知と問い合わせ
	signals::broadcaster& get_broadcaster() { return broadcaster; }
	signals::contactor& get_contactor() { return contactor; }

public:
	explicit scene_manager(engine_context& context);
	~scene_manager();

	scene_manager(const scene_manager&) = delete;
	scene_manager& operator=(const scene_manager&) = delete;

public:
	//	更新
	void updates(float elapsed_time);
	void post_updates(float elapsed_time);

#if defined(USE_IMGUI)
	void imgui_render();
#endif	//	defined(USE_IMGUI)

public:
	//	シーン追加リクエスト
	bool add_scene(const scene_resource_ptr& resource);
	bool add_scene(std::string_view file_path);

	//	シーン破棄リクエスト
	bool remove_scene(const scene_resource_ptr& resource);
	bool remove_scene(std::string_view scene_name);

	//	シーン存在判定
	bool valid_scene(const scene_resource_ptr& resource) const;
	bool valid_scene(std::string_view scene_name) const;

	//	全シーン破棄
	void remove_all_scene();

	//	シーン検索
	scene_instance* find_scene(std::string_view scene_name) const;

public:
	//	シーン名生成
	static std::string make_scene_name(const scene_resource_ptr& resource);

private:
	//	シーンリソース読み込み
	static scene_resource_ptr load_resource(std::string_view file_path);

	//	追加可能か判定
	bool can_add_scene(std::string_view name) const;

private:
	engine_context& context;

	signals::broadcaster broadcaster;
	signals::contactor contactor;

	std::vector<add_scene_data> add_scenes;
	std::vector<std::string> remove_scenes;
	std::vector<std::pair<std::string, std::unique_ptr<scene_instance>>> scenes;
};
