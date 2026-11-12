#include "Object/game_object_serializer.h"

#include <istream>
#include <iterator>
#include <ostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Object/game_object_registry.h"
#include "Object/spatial_object.h"
#include "Resource/asset_registry.h"

namespace
{
	using object_table = std::unordered_map<universally_unique_identifier, game_object_ptr>;

	//	読込中のファイルで生成したオブジェクトの表を持つアーカイブ
	class input_archive : public cereal::JSONInputArchive
	{
	public:
		explicit input_archive(std::istream& stream)
		: cereal::JSONInputArchive(stream)
		{
		}

		object_table table;
	};

	//	保存対象のオブジェクトを持つアーカイブ
	class output_archive : public cereal::JSONOutputArchive
	{
	public:
		explicit output_archive(std::ostream& stream)
		: cereal::JSONOutputArchive(stream)
		{
		}

		std::unordered_set<const game_object*> members;
	};

	universally_unique_identifier from_text(const std::string& text)
	{
		universally_unique_identifier uuid;
		if (!text.empty())
			universally_unique_identifier::from_string(text, uuid);
		return uuid;
	}
}

bool game_object_serializer::save(std::ostream& stream, const std::vector<game_object_ptr>& objects, const write_function& write_extra)
{
	const game_object_registry* registry = game_object_registry::instance();
	assert(registry && "game_object_registry が作られていない");
	if (!registry)
		return false;

	//	保存
	std::vector<game_object_ptr> targets;
	for (const game_object_ptr& object : objects)
	{
		if (object && !object->is_transient() && registry->contains(object->get_type_name()))
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
		output_archive archive(stream);
		for (const game_object_ptr& object : targets)
			archive.members.insert(object.get());

		if (write_extra)
			write_extra(archive);


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

bool game_object_serializer::load(std::istream& stream, std::vector<game_object_ptr>& out_objects, const read_function& read_extra)
{
	const game_object_registry* registry = game_object_registry::instance();
	assert(registry && "game_object_registry が作られていない");
	if (!registry)
		return false;

	std::vector<game_object_ptr> objects;
	try
	{
		input_archive archive(stream);


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
				archive.table.insert_or_assign(uuid, object);
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
		if (object_count != created.size())
			return false;
		for (cereal::size_type index = 0; index < object_count; ++index)
		{
			archive.startNode();

			if (game_object_ptr& object = created[index])
			{
				std::string parent;
				archive(CEREAL_NVP(parent));
				object->deserialize(archive);

				//	親は同じファイル内から引く
				const universally_unique_identifier parent_uuid = from_text(parent);
				if (!parent_uuid.is_empty())
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

		if (read_extra)
			read_extra(archive);
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

	auto it = loading->table.find(uuid);
	return it != loading->table.end() ? it->second : nullptr;
}

void game_object_serializer::save_asset_reference(cereal::JSONOutputArchive& archive, const char* name, const universally_unique_identifier& uuid)
{
	archive.setNextName(name);
	save_asset_reference(archive, uuid);
}

universally_unique_identifier game_object_serializer::load_asset_reference(cereal::JSONInputArchive& archive, const char* name)
{
	archive.setNextName(name);
	return load_asset_reference(archive);
}

void game_object_serializer::save_asset_reference(cereal::JSONOutputArchive& archive, const universally_unique_identifier& uuid)
{
	std::string uuid_text = uuid.is_empty() ? std::string() : uuid.to_string();
	archive(uuid_text);
}

universally_unique_identifier game_object_serializer::load_asset_reference(cereal::JSONInputArchive& archive)
{
	std::string uuid_text;
	archive(uuid_text);
	return from_text(uuid_text);
}

std::string game_object_serializer::resolve_asset_path(const universally_unique_identifier& uuid)
{
	return asset_registry::is_valid() ? asset_registry::instance()->find_path(uuid) : std::string();
}

void game_object_serializer::save_object(cereal::JSONOutputArchive& archive, const char* name, const game_object_ptr& object)
{
	//	保存対象に無いオブジェクトは参照しない
	std::string uuid;
	if (object)
	{
		const output_archive* saving = dynamic_cast<const output_archive*>(&archive);
		assert(saving && "game_object_serializer::save 以外から保存している");
		if (saving && saving->members.contains(object.get()))
			uuid = object->get_uuid().to_string();
		else
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "game_object_serializer reference outside the file is not saved[%s]", name);
	}
	archive(cereal::make_nvp(name, uuid));
}

game_object_ptr game_object_serializer::load_object(cereal::JSONInputArchive& archive, const char* name)
{
	std::string uuid;
	archive(cereal::make_nvp(name, uuid));
	return find_object(archive, from_text(uuid));
}
