#pragma once

#include <memory>
#include <vector>

#include "Scenes/scene_resource.h"
#include "Scenes/scene_system.h"

class engine_context;

//	シーンインスタンス
class scene_instance
{
public:
	//	読み込んだシーンリソースごとのオブジェクトとシステム
	struct loaded_scene
	{
		scene_resource_ptr resource;
		std::vector<std::weak_ptr<game_object>> objects;
		std::unique_ptr<scene_system> system;
	};

public:
	//	エンジンコンテキスト取得
	engine_context& get_context() const { return context; }
	const scene_resource_ptr& get_resource() const { return resource; }
	const std::vector<loaded_scene>& get_loaded_scenes() const { return loaded_scenes; }

public:
	scene_instance(engine_context& context, scene_resource_ptr resource);
	~scene_instance();

	scene_instance(const scene_instance&) = delete;
	scene_instance& operator=(const scene_instance&) = delete;

	bool initialize();
	void uninitialize();

	void pre_update(float elapsed_time);
	void post_update(float elapsed_time);
#if defined(USE_IMGUI)
	void imgui_render();
#endif	//	defined(USE_IMGUI)

public:
	//	システムを検索
	template<class Type>
	Type* find_system() const
	{
		for (const loaded_scene& scene : loaded_scenes)
		{
			if (auto system = dynamic_cast<Type*>(scene.system.get()))
				return system;
		}
		return nullptr;
	}

public:
	//	実行時だけのオブジェクトの登録/解除
	void register_transient_object(game_object_ptr object);
	void register_transient_object(collision_object_ptr object);
	void unregister_transient_object(const game_object_ptr& object);

private:
	engine_context& context;
	scene_resource_ptr resource;
	bool initialized = false;

	std::vector<loaded_scene> loaded_scenes;
	std::vector<std::weak_ptr<game_object>> transient_objects;
	std::vector<std::weak_ptr<collision_object>> collision_objects;
};
