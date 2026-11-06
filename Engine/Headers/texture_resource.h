#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "texture.h"
#include "texture_import_settings.h"
#include "Resource/asset_reference.h"
#include "Resource/resource_base.h"
#include "Resource/resource_loader.h"
#include "Resource/resource_saver.h"

class texture_resource;
class texture_importer;

namespace resource
{
	template<>
	class loader<texture_resource>;

	template<>
	class saver<texture_resource>;
}	//	namespace resource

//	テクスチャリソース
class texture_resource : public resource::resource_base
{
	friend class resource::loader<texture_resource>;
	friend class resource::saver<texture_resource>;
	friend class texture_importer;
	friend class texture_factory;

public:
	static constexpr std::string_view extension = "etx";

public:
	const texture_ptr& get_texture() const { return value; }
	const texture_import_settings& get_settings() const { return settings; }

private:
	bool load_file(std::string_view file_path);
	bool save_file(std::string_view file_path, std::string_view dds);

private:
	texture_import_settings settings;
	texture_ptr value;
};

using texture_resource_ptr = std::shared_ptr<texture_resource>;

namespace resource
{
	template<>
	class loader<texture_resource> : public loader_base
	{
	public:
		operation_state load(std::string_view file_path) override;
		std::shared_ptr<void> get_resource() const override { return resource; }

		static texture_resource_ptr create_fallback(const asset_reference<texture_resource>& reference);

		bool revert(texture_resource& target);

	private:
		texture_resource_ptr resource;
	};

	template<>
	class saver<texture_resource>
	{
	public:
		bool save(texture_resource& resource, std::string_view file_path);
		bool save(texture_resource& resource, std::string_view file_path, std::string_view dds);
	};
}	//	namespace resource
