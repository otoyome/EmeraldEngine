#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Resource/resource_base.h"
#include "Resource/resource_loader.h"
#include "Resource/resource_saver.h"
#include "Resource/asset_handle.h"

class prefab_resource;
using prefab_resource_ptr = std::shared_ptr<prefab_resource>;

namespace resource
{
	template<>
	class loader<prefab_resource>;

	template<>
	class saver<prefab_resource>;
}	//	namespace resource

//	プレハブリソース
class prefab_resource : public resource::resource_base
{
	friend class resource::loader<prefab_resource>;
	friend class resource::saver<prefab_resource>;

public:
	static constexpr std::string_view extension = "eprefab";

	//	生成結果
	struct instance
	{
		game_object_ptr root;					//	ルートオブジェクト
		std::vector<game_object_ptr> objects;	//	生成した全オブジェクト
	};

public:
	prefab_resource() = default;
	~prefab_resource() override = default;

	const std::string& get_body() const { return body; }

	//	オブジェクト登録
	bool set_contents(const std::vector<game_object_ptr>& objects);

	//	オブジェクト生成
	bool instantiate(instance& out_instance) const;

private:
	bool save_file(std::string_view file_path);
	bool load_file(std::string_view file_path);

private:
	std::string body;
};

namespace resource
{
	template<>
	class loader<prefab_resource> : public loader_base
	{
	public:
		operation_state load(std::string_view file_path) override;
		std::shared_ptr<void> get_resource() const override { return resource; }

	private:
		prefab_resource_ptr resource;
	};

	template<>
	class saver<prefab_resource>
	{
	public:
		bool save(prefab_resource& resource, std::string_view file_path);
	};
}	//	namespace resource
