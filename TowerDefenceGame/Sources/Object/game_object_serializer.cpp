#include "Object/game_object_serializer.h"

#include <istream>
#include <ostream>
#include <unordered_map>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Object/game_object_registry.h"
#include "Object/spatial_object.h"
#include "Resource/asset_registry.h"

namespace
{
	//	読込中のファイルで生成したオブジェクトの表を持つアーカイブ
	class input_archive : public cereal::JSONInputArchive
	{
	public:
		explicit input_archive(std::istream& stream) : cereal::JSONInputArchive(stream) {}

		std::unordered_map<universally_unique_identifier, game_object_ptr> objects;
	};
}

bool game_object_serializer::save(std::ostream& stream, const std::vector<game_object_ptr>& objects)
{
	const game_object_registry* registry = game_object_registry::instance();
	assert(registry && "game_object_registry が作られていない");
	if (!registry)
		return false;

	//	保存
	std::vector<game_object_ptr> targets;
	for (const game_object_ptr& object : objects)
	{
		if (object && registry->contains(object->get_type_name()))
			targets.emplace_back(object);
	}

	//	親の特定用に持ち主のUUIDを取得
	std::unordered_map<const spatial*, universally_unique_identifier> spatial_owners;
	for (const game_object_ptr& object : targets)
	{
		if (auto spatial_target = std::dynamic_pointer_cast<spatial_object>(object))
			spatial_owners.emplace(spatial_target->get_spatial().get(), object->get_uuid());
	}

	try
	{
		cereal::JSONOutputArchive archive(stream);

		std::uint32_t version = current_version;
		archive(CEREAL_NVP(version));

		//	先にオブジェクトの型とUUIDを保存し、読込時はこれで全て生成してから中身を読む
		archive.setNextName("entities");
		archive.startNode();
		archive.makeArray();
		for (const game_object_ptr& object : targets)
		{
			std::string type(object->get_type_name());
			std::string uuid = object->get_uuid().to_string();
			archive.startNode();
			archive(CEREAL_NVP(type), CEREAL_NVP(uuid));
			archive.finishNode();
		}
		archive.finishNode();

		//	中身は entities と同じ順番で保存する
		archive.setNextName("objects");
		archive.startNode();
		archive.makeArray();
		for (const game_object_ptr& object : targets)
		{
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
			archive(CEREAL_NVP(parent));
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

	try
	{
		input_archive archive(stream);

		std::uint32_t version = 0;
		archive(CEREAL_NVP(version));
		if (version > current_version)
			return false;

		//	全オブジェクトを先に生成する
		archive.setNextName("entities");
		archive.startNode();
		cereal::size_type entity_count = 0;
		archive.loadSize(entity_count);
		std::vector<game_object_ptr> created(entity_count);
		for (cereal::size_type index = 0; index < entity_count; ++index)
		{
			archive.startNode();

			std::string type;
			std::string uuid_text;
			archive(cereal::make_nvp("type", type), cereal::make_nvp("uuid", uuid_text));

			universally_unique_identifier uuid;
			game_object_ptr object = registry->create(type);
			if (object && universally_unique_identifier::from_string(uuid_text, uuid))
			{
				object->set_uuid(uuid);
				archive.objects.insert_or_assign(uuid, object);
				created[index] = std::move(object);
			}

			archive.finishNode();
		}
		archive.finishNode();

		//	中身を生成
		archive.setNextName("objects");
		archive.startNode();
		cereal::size_type object_count = 0;
		archive.loadSize(object_count);
		if (object_count != entity_count)
			return false;
		for (cereal::size_type index = 0; index < object_count; ++index)
		{
			archive.startNode();

			if (game_object_ptr& object = created[index])
			{
				std::string parent;
				archive(CEREAL_NVP(parent));
				object->deserialize(archive);

				universally_unique_identifier parent_uuid;
				if (!parent.empty() && universally_unique_identifier::from_string(parent, parent_uuid))
				{
					auto child = std::dynamic_pointer_cast<spatial_object>(object);
					auto parent_object = std::dynamic_pointer_cast<spatial_object>(find_object(archive, parent_uuid));
					if (child && parent_object)
						child->attach(parent_object, spatial_object::attach_rule::keep_relative);
				}

				objects.emplace_back(object);
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

	out_objects = std::move(objects);
	return true;
}

game_object_ptr game_object_serializer::find_object(cereal::JSONInputArchive& archive, const universally_unique_identifier& uuid)
{
	if (uuid.is_empty())
		return nullptr;

	const input_archive* loading = dynamic_cast<const input_archive*>(&archive);
	assert(loading && "game_object_serializer::load 以外から読み込んでいる");
	if (!loading)
		return nullptr;

	auto it = loading->objects.find(uuid);
	return it != loading->objects.end() ? it->second : nullptr;
}

void game_object_serializer::save_asset_reference(cereal::JSONOutputArchive& archive, const char* name, const universally_unique_identifier& uuid, std::string_view path)
{
	std::string uuid_text = uuid.is_empty() ? std::string() : uuid.to_string();
	std::string path_text(path);
	archive.setNextName(name);
	archive.startNode();
	archive(cereal::make_nvp("uuid", uuid_text), cereal::make_nvp("path", path_text));
	archive.finishNode();
}

void game_object_serializer::load_asset_reference(cereal::JSONInputArchive& archive, const char* name, universally_unique_identifier& out_uuid, std::string& out_path)
{
	std::string uuid_text;
	archive.setNextName(name);
	archive.startNode();
	archive(cereal::make_nvp("uuid", uuid_text), cereal::make_nvp("path", out_path));
	archive.finishNode();

	out_uuid.clear();
	if (!uuid_text.empty())
		universally_unique_identifier::from_string(uuid_text, out_uuid);
}

std::string game_object_serializer::resolve_asset_path(const universally_unique_identifier& uuid, std::string_view path)
{
	if (asset_registry::is_valid())
		return asset_registry::instance()->resolve_path(uuid, path);
	return std::string(path);
}

void game_object_serializer::save_object_uuid(cereal::JSONOutputArchive& archive, const char* name, const universally_unique_identifier& uuid)
{
	std::string uuid_text = uuid.is_empty() ? std::string() : uuid.to_string();
	archive(cereal::make_nvp(name, uuid_text));
}

void game_object_serializer::load_object_uuid(cereal::JSONInputArchive& archive, const char* name, universally_unique_identifier& out_uuid)
{
	std::string uuid_text;
	archive(cereal::make_nvp(name, uuid_text));

	out_uuid.clear();
	if (!uuid_text.empty())
		universally_unique_identifier::from_string(uuid_text, out_uuid);
}
