#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Resource/resource_base.h"
#include "Resource/resource_loader.h"
#include "Resource/resource_saver.h"
#include "Resource/asset_reference.h"
#include "Scenes/scene_resource.h"

class scene_set_resource;
using scene_set_resource_ptr = std::shared_ptr<scene_set_resource>;

namespace resource
{
	template<>
	class loader<scene_set_resource>;

	template<>
	class saver<scene_set_resource>;
}	//	namespace resource

//	シーンセット
class scene_set_resource : public resource::resource_base
{
	friend class resource::loader<scene_set_resource>;
	friend class resource::saver<scene_set_resource>;

public:
	static constexpr std::string_view extension = "esceneset";

public:
	//	シーンの参照の一覧
	const std::vector<resource::asset_reference<scene_resource>>& get_scenes() const { return scenes; }
	//	シーンの一覧を設定
	void set_scenes(const std::vector<scene_resource_ptr>& resources);
	//	シーンの参照の一覧を設定
	void set_scene_references(std::vector<resource::asset_reference<scene_resource>> references) { scenes = std::move(references); }

	//	シーンのファイルパスの一覧
	std::vector<std::string> get_scene_paths() const;

public:
	scene_set_resource() = default;
	~scene_set_resource() override = default;

public:
	//	シーンセットのファイルか?
	static bool is_scene_set_path(std::string_view file_path);

private:
	bool save_file(std::string_view file_path);
	bool load_file(std::string_view file_path);

private:
	std::vector<resource::asset_reference<scene_resource>> scenes;
};

namespace resource
{
	template<>
	class loader<scene_set_resource> : public loader_base
	{
	public:
		operation_state load(std::string_view file_path) override;
		std::shared_ptr<void> get_resource() const override { return resource; }

	private:
		scene_set_resource_ptr resource;
	};

	template<>
	class saver<scene_set_resource>
	{
	public:
		bool save(scene_set_resource& resource, std::string_view file_path);
	};
}	//	namespace resource
