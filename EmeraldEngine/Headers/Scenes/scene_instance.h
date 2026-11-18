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
	//	エンジンコンテキスト取得
	engine_context& get_context() const { return context; }
	const scene_resource_ptr& get_resource() const { return resource; }
	const std::vector<std::weak_ptr<game_object>>& get_objects() const { return objects; }
	scene_system* get_system() const { return system.get(); }

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
		return dynamic_cast<Type*>(system.get());
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

	std::vector<std::weak_ptr<game_object>> objects;
	std::unique_ptr<scene_system> system;
	std::vector<std::weak_ptr<game_object>> transient_objects;
	std::vector<std::weak_ptr<collision_object>> collision_objects;
};
