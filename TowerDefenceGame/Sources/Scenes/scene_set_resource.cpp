#include "Scenes/scene_set_resource.h"

#include <filesystem>
#include <sstream>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Resource/asset_file_header.h"
#include "Resource/asset_registry.h"
#include "Object/game_object_serializer.h"

bool scene_set_resource::is_scene_set_path(std::string_view file_path)
{
	return resource::get_asset_type_from_extension(std::filesystem::path(std::string(file_path)).extension().string()) == resource::asset_types::scene_set;
}

void scene_set_resource::set_scenes(const std::vector<scene_resource_ptr>& resources)
{
	scenes.clear();
	for (const scene_resource_ptr& resource : resources)
	{
		if (resource)
			scenes.emplace_back(resource::asset_reference<scene_resource>::from(resource));
	}
}

std::vector<std::string> scene_set_resource::get_scene_paths() const
{
	std::vector<std::string> result;
	for (const resource::asset_reference<scene_resource>& reference : scenes)
	{
		std::string path = asset_registry::is_valid() ? asset_registry::instance()->resolve_path(reference.uuid, reference.path) : reference.path;
		if (path.empty())
		{
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "scene_set_resource scene not found[%s]", std::string(get_file_path()).c_str());
			continue;
		}
		result.emplace_back(std::move(path));
	}
	return result;
}

bool scene_set_resource::save_file(std::string_view file_path)
{
	std::ostringstream stream;
	{
		cereal::JSONOutputArchive archive(stream);
		archive.setNextName("scenes");
		archive.startNode();
		archive.makeArray();
		for (const resource::asset_reference<scene_resource>& reference : scenes)
			game_object_serializer::save_asset_reference(archive, reference.uuid);
		archive.finishNode();
	}
	if (!resource::write_asset_file(*this, resource::asset_types::scene_set, file_path, stream.str()))
		return false;
	set_file_path(file_path);
	return true;
}

bool scene_set_resource::load_file(std::string_view file_path)
{
	resource::asset_file_header header;
	std::vector<char> data;
	if (!resource::read_asset_file(file_path, resource::asset_types::scene_set, header, data))
		return false;

	scenes.clear();
	try
	{
		std::istringstream stream(std::string(data.begin(), data.end()));
		cereal::JSONInputArchive archive(stream);
		archive.setNextName("scenes");
		archive.startNode();
		cereal::size_type count = 0;
		archive.loadSize(count);
		for (cereal::size_type index = 0; index < count; ++index)
		{
			resource::asset_reference<scene_resource>& reference = scenes.emplace_back();
			reference.uuid = game_object_serializer::load_asset_reference(archive);
		}
		archive.finishNode();
	}
	catch (const cereal::Exception&)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_set_resource deserialize failed[%s]", std::string(file_path).c_str());
		scenes.clear();
		return false;
	}

	inherit_uuid(header.uuid);
	set_file_path(file_path);
	return true;
}

namespace resource
{
	operation_state loader<scene_set_resource>::load(std::string_view file_path)
	{
		std::error_code error;
		if (!std::filesystem::exists(std::filesystem::path(std::string(file_path)), error))
		{
			set_error_message("file not found");
			return operation_state::not_found;
		}

		resource = std::make_shared<scene_set_resource>();
		if (!resource->load_file(file_path))
			return operation_state::deserialize_failed;
		return operation_state::success;
	}

	bool saver<scene_set_resource>::save(scene_set_resource& resource, std::string_view file_path)
	{
		return resource.save_file(file_path);
	}
}	//	namespace resource
