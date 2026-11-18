#include "Object/prefab_resource.h"

#include <filesystem>
#include <sstream>

#include "Resource/asset_file_header.h"
#include "Object/game_object_serializer.h"
#include "Object/spatial_object.h"

bool prefab_resource::set_contents(const std::vector<game_object_ptr>& objects)
{
	std::ostringstream stream;
	if (!game_object_serializer::save(stream, objects))
		return false;

	body = stream.str();
	return true;
}

bool prefab_resource::instantiate(instance& out_instance) const
{
	std::vector<game_object_ptr> objects;
	std::istringstream stream(body);
	if (!game_object_serializer::load(stream, objects))
		return false;

	//	UUID ‚¾‚¯U‚è’¼‚·
	for (const game_object_ptr& object : objects)
		object->set_uuid(universally_unique_identifier::generate());

	//	ƒ‹[ƒgæ“¾
	game_object_ptr root;
	for (const game_object_ptr& object : objects)
	{
		auto spatial_target = std::dynamic_pointer_cast<spatial_object>(object);
		if (!spatial_target || !spatial_target->get_spatial()->get_parent())
		{
			root = object;
			break;
		}
	}

	out_instance.root = std::move(root);
	out_instance.objects = std::move(objects);
	return true;
}

bool prefab_resource::save_file(std::string_view file_path)
{
	return resource::write_asset_file(*this, resource::asset_types::prefab, file_path, body);
}

bool prefab_resource::load_file(std::string_view file_path)
{
	resource::asset_file_header header;
	std::vector<char> data;
	if (!resource::read_asset_file(file_path, resource::asset_types::prefab, header, data))
		return false;

	body.assign(data.begin(), data.end());
	inherit_uuid(header.uuid);
	set_file_path(file_path);
	return true;
}

namespace resource
{
	operation_state loader<prefab_resource>::load(std::string_view file_path)
	{
		std::error_code error;
		if (!std::filesystem::exists(std::filesystem::path(std::string(file_path)), error))
		{
			set_error_message("file not found");
			return operation_state::not_found;
		}

		resource = std::make_shared<prefab_resource>();
		if (!resource->load_file(file_path))
			return operation_state::deserialize_failed;
		return operation_state::success;
	}

	bool saver<prefab_resource>::save(prefab_resource& resource, std::string_view file_path)
	{
		return resource.save_file(file_path);
	}
}	//	namespace resource
