#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Resource/resource_base.h"
#include "Resource/resource_loader.h"
#include "Resource/resource_saver.h"
#include "Resource/asset_handle.h"

class scene_resource;
class scene_system;
using scene_resource_ptr = std::shared_ptr<scene_resource>;

namespace resource
{
	template<>
	class loader<scene_resource>;

	template<>
	class saver<scene_resource>;
}	//	namespace resource

//	シーンリソース
class scene_resource : public resource::resource_base
{
	friend class resource::loader<scene_resource>;
	friend class resource::saver<scene_resource>;

public:
	static constexpr std::string_view extension = "escene";

public:
	scene_resource() = default;
	~scene_resource() override = default;

	//	サブシーンリスト
	const std::vector<resource::asset_handle<scene_resource>>& get_sub_scenes() const { return sub_scenes; }

	//	JSON文字列取得
	const std::string& get_body() const { return body; }


	//	保存・編集した結果を設定
	bool set_contents(const std::vector<resource::asset_handle<scene_resource>>& scenes, const std::vector<game_object_ptr>& objects,
					  const scene_system* system = nullptr);

private:
	bool save_file(std::string_view file_path);
	bool load_file(std::string_view file_path);
	bool read_sub_scenes();

private:
	std::string body;
	std::vector<resource::asset_handle<scene_resource>> sub_scenes;
};

namespace resource
{
	template<>
	class loader<scene_resource> : public loader_base
	{
	public:
		operation_state load(std::string_view file_path) override;
		std::shared_ptr<void> get_resource() const override { return resource; }

	private:
		scene_resource_ptr resource;
	};

	template<>
	class saver<scene_resource>
	{
	public:
		bool save(scene_resource& resource, std::string_view file_path);
	};
}	//	namespace resource
