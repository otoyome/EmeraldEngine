#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Scenes/scene_resource.h"

class scene_system;

namespace scene_resource_utility
{
	//	作成シーン情報
	struct created_scene
	{
		std::vector<game_object_ptr> objects;
		std::unique_ptr<scene_system> system;
	};

	//	複数のシーンのオブジェクトとシステムをまとめて生成
	bool create(const std::vector<scene_resource_ptr>& scenes, std::vector<created_scene>& out_scenes);

	//	親と、親に登録されたサブシーンを読んで並べる
	void collect_scenes(const scene_resource_ptr& root, std::vector<scene_resource_ptr>& out_scenes);

	//	表示用のシーン名
	std::string make_display_name(std::string_view path);
}	//	namespace scene_resource_utility
