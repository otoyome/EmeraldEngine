#include "Object/game_object_serializer.h"

#include <istream>
#include <ostream>
#include <unordered_map>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Object/game_object_registry.h"
#include "Object/spatial_object.h"

bool game_object_serializer::save(std::ostream& stream, const std::vector<game_object_ptr>& objects)
{
	const game_object_registry* registry = game_object_registry::instance();
	assert(registry && "game_object_registry が作られていない");
	if (!registry)
		return false;

	//	親の特定用に持ち主のUUIDを取得
	std::unordered_map<const spatial*, universally_unique_identifier> spatial_owners;
	for (const game_object_ptr& object : objects)
	{
		if (!object || !registry->contains(object->get_type_name()))
			continue;

		if (auto spatial_target = std::dynamic_pointer_cast<spatial_object>(object))
			spatial_owners.emplace(spatial_target->get_spatial().get(), object->get_uuid());
	}

	try
	{
		cereal::JSONOutputArchive archive(stream);

		std::uint32_t version = current_version;
		archive(CEREAL_NVP(version));

		archive.setNextName("objects");
		archive.startNode();
		archive.makeArray();
		for (const game_object_ptr& object : objects)
		{
			if (!object || !registry->contains(object->get_type_name()))
				continue;

			std::string type(object->get_type_name());
			std::string parent;
			if (auto spatial_target = std::dynamic_pointer_cast<spatial_object>(object))
			{
				if (auto parent_spatial = spatial_target->get_spatial()->get_parent())
				{
					auto it = spatial_owners.find(parent_spatial.get());
					if (it != spatial_owners.end())
						parent = it->second.to_string();
				}
			}

			archive.startNode();
			archive(CEREAL_NVP(type), CEREAL_NVP(parent));
			object->serialize(archive);
			archive.finishNode();
		}
		archive.finishNode();
	}
	catch (const cereal::Exception& exception)
	{
		_ASSERT_EXPR(false, exception.what());
		return false;
	}

	return true;
}

bool game_object_serializer::load(std::istream& stream, std::vector<game_object_ptr>& out_objects)
{
	const game_object_registry* registry = game_object_registry::instance();
	assert(registry && "game_object_registry が作られていない");
	if (!registry)
		return false;

	std::vector<game_object_ptr> objects;
	std::unordered_map<universally_unique_identifier, game_object_ptr> uuid_table;
	std::vector<std::pair<std::shared_ptr<spatial_object>, universally_unique_identifier>> parent_links;

	try
	{
		cereal::JSONInputArchive archive(stream);

		std::uint32_t version = 0;
		archive(CEREAL_NVP(version));
		if (version > current_version)
			return false;

		archive.setNextName("objects");
		archive.startNode();
		cereal::size_type count = 0;
		archive.loadSize(count);
		for (cereal::size_type index = 0; index < count; ++index)
		{
			archive.startNode();

			std::string type;
			std::string parent;
			archive(CEREAL_NVP(type), CEREAL_NVP(parent));

			game_object_ptr object = registry->create(type);
			if (object)
			{
				object->deserialize(archive);
				uuid_table.insert_or_assign(object->get_uuid(), object);

				universally_unique_identifier parent_uuid;
				if (!parent.empty() && universally_unique_identifier::from_string(parent, parent_uuid))
				{
					if (auto spatial_target = std::dynamic_pointer_cast<spatial_object>(object))
						parent_links.emplace_back(spatial_target, parent_uuid);
				}

				objects.emplace_back(std::move(object));
			}

			archive.finishNode();
		}
		archive.finishNode();
	}
	catch (const cereal::Exception& exception)
	{
		_ASSERT_EXPR(false, exception.what());
		return false;
	}

	//	全て生成してから親子付けする
	for (auto& [child, parent_uuid] : parent_links)
	{
		auto it = uuid_table.find(parent_uuid);
		if (it == uuid_table.end())
			continue;

		if (auto parent = std::dynamic_pointer_cast<spatial_object>(it->second))
			child->attach(parent, spatial_object::attach_rule::keep_relative);
	}

	out_objects = std::move(objects);
	return true;
}
