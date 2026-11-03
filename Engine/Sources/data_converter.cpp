#include "data_converter.h"

#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "animation_data_version.h"

#include "Resource/resource_manager.h"
#include "Resource/asset_file_header.h"

#if defined(USE_IMGUI)

#include <filesystem>
#include <unordered_set>

namespace
{
}

void data_converter::assign_material_paths(const std::vector<material_resource_ptr>& materials, std::string_view owner_file_path)
{
	const std::wstring extension = L"." + std::filesystem::path(std::string(material_resource::extension)).wstring();
	const std::filesystem::path directory = std::filesystem::path(std::string(owner_file_path)).parent_path() / std::string(material_directory_name);

	std::unordered_set<std::wstring> assigned_paths;
	for (auto& material : materials)
	{
		if (!material || material->is_missing() || !material->get_file_path().empty())
			continue;

		//	 SJIS の2バイト目に区切り文字等が含まれるため、ワイド文字にしてから置き換える
		std::wstring file_name = std::filesystem::path(std::string(material->get_name())).wstring();
		for (auto& c : file_name)
		{
			if (c < 0x20 || std::wstring_view(L"\\/:*?\"<>|").find(c) != std::wstring_view::npos)
				c = L'_';
		}
		if (file_name.empty())
			file_name = L"material";

		std::filesystem::path path = directory / (file_name + extension);
		for (int number = 1; std::filesystem::exists(path) || assigned_paths.contains(path.generic_wstring()); ++number)
		{
			path = directory / (file_name + L"_" + std::to_wstring(number) + extension);
		}
		assigned_paths.insert(path.generic_wstring());
		material->set_file_path(std::filesystem::path(path.generic_wstring()).string());
	}
}

void data_converter::inherit_existing_model(model_resource& resource)
{
	const std::string file_path(resource.get_file_path());
	::resource::asset_file_header header;
	if (file_path.empty() || !::resource::read_asset_header(std::filesystem::path(file_path), header) || header.type != ::resource::asset_types::model)
		return;

	resource.inherit_uuid(header.uuid);

	auto operation = resource_manager::instance()->load<model_resource>(file_path);
	if (!operation->is_succeeded())
		return;

	std::unordered_set<const material_resource*> used_materials;
	for (auto& material : resource.get_materials_for_editor())
	{
		if (!material)
			continue;

		for (auto& existing_material : operation->get_resource()->get_materials())
		{
			if (!existing_material || existing_material->is_missing() || used_materials.contains(existing_material.get()))
				continue;
			if (existing_material->get_name() != material->get_name())
				continue;

			used_materials.insert(existing_material.get());
			material = existing_material;
			break;
		}
	}
}

#endif	//	defined(USE_IMGUI)
