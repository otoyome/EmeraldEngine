#include "Scenes/scene_resource.h"

#include <filesystem>
#include <sstream>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Resource/asset_file_header.h"
#include "Object/game_object_serializer.h"
#include "Scenes/scene_system.h"

namespace
{
	void write_system(cereal::JSONOutputArchive& archive, const scene_system* system)
	{
		std::string type = system ? std::string(system->get_type_name()) : std::string();
		archive.setNextName("system");
		archive.startNode();
		archive(CEREAL_NVP(type));
		if (system)
			system->serialize(archive);
		archive.finishNode();
	}
}

bool scene_resource::set_contents(const std::vector<resource::asset_handle<scene_resource>>& scenes, const std::vector<game_object_ptr>& objects,
								  const scene_system* system)
{
	std::ostringstream stream;
	const bool result = game_object_serializer::save(stream, objects, [&](cereal::JSONOutputArchive& archive)
	{
		//	サブシーンは UUID 文字列の配列で保存する
		archive.setNextName("sub_scenes");
		archive.startNode();
		archive.makeArray();
		for (const auto& scene : scenes)
		{
			game_object_serializer::save_asset_reference(archive, scene.get_reference().uuid);
		}
		archive.finishNode();

		write_system(archive, system);
	});
	if (!result)
		return false;

	body = stream.str();
	sub_scenes = scenes;
	return true;
}

bool scene_resource::save_file(std::string_view file_path)
{
	return resource::write_asset_file(*this, resource::asset_types::scene, file_path, body);
}

bool scene_resource::load_file(std::string_view file_path)
{
	resource::asset_file_header header;
	std::vector<char> data;
	if (!resource::read_asset_file(file_path, resource::asset_types::scene, header, data))
		return false;

	body.assign(data.begin(), data.end());
	if (!read_sub_scenes())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "scene_resource deserialize failed[%s]", std::string(file_path).c_str());
		return false;
	}

	inherit_uuid(header.uuid);
	set_file_path(file_path);
	return true;
}

bool scene_resource::read_sub_scenes()
{
	sub_scenes.clear();
	try
	{
		std::istringstream stream(body);
		cereal::JSONInputArchive archive(stream);

		archive.setNextName("sub_scenes");
		archive.startNode();
		cereal::size_type count = 0;
		archive.loadSize(count);
		for (cereal::size_type index = 0; index < count; ++index)
		{
			resource::asset_reference<scene_resource> reference;
			reference.uuid = game_object_serializer::load_asset_reference(archive);
			sub_scenes.emplace_back().set_reference(std::move(reference));
		}
		archive.finishNode();
	}
	catch (const cereal::Exception&)
	{
		sub_scenes.clear();
		return false;
	}
	return true;
}

namespace resource
{
	operation_state loader<scene_resource>::load(std::string_view file_path)
	{
		std::error_code error;
		if (!std::filesystem::exists(std::filesystem::path(std::string(file_path)), error))
		{
			set_error_message("file not found");
			return operation_state::not_found;
		}

		resource = std::make_shared<scene_resource>();
		if (!resource->load_file(file_path))
			return operation_state::deserialize_failed;
		return operation_state::success;
	}

	bool saver<scene_resource>::save(scene_resource& resource, std::string_view file_path)
	{
		return resource.save_file(file_path);
	}
}	//	namespace resource
