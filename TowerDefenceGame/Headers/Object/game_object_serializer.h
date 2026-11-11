#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

//	game_objectのシリアライザー
class game_object_serializer
{
public:
	static constexpr std::uint32_t current_version = 1;

public:
	//	保存
	static bool save(std::ostream& stream, const std::vector<game_object_ptr>& objects);

	//	読込
	static bool load(std::istream& stream, std::vector<game_object_ptr>& out_objects);

public:
	//	アセットへの参照を { "uuid", "path" } として読み書きする
	static void save_asset_reference(cereal::JSONOutputArchive& archive, const char* name, const universally_unique_identifier& uuid, std::string_view path);
	static void load_asset_reference(cereal::JSONInputArchive& archive, const char* name, universally_unique_identifier& out_uuid, std::string& out_path);

	//	UUID からパスを引く
	static std::string resolve_asset_path(const universally_unique_identifier& uuid, std::string_view path);

	//	同じファイル内の参照を UUID 文字列として保存
	template<class Type>
	static void save_object_reference(cereal::JSONOutputArchive& archive, const char* name, const std::shared_ptr<Type>& object)
	{
		save_object_uuid(archive, name, object ? object->get_uuid() : universally_unique_identifier());
	}
	template<class Type>
	static void load_object_reference(cereal::JSONInputArchive& archive, const char* name, std::shared_ptr<Type>& out_object)
	{
		universally_unique_identifier uuid;
		load_object_uuid(archive, name, uuid);
		out_object = std::dynamic_pointer_cast<Type>(find_object(archive, uuid));
	}

private:
	static void save_object_uuid(cereal::JSONOutputArchive& archive, const char* name, const universally_unique_identifier& uuid);
	static void load_object_uuid(cereal::JSONInputArchive& archive, const char* name, universally_unique_identifier& out_uuid);
	static game_object_ptr find_object(cereal::JSONInputArchive& archive, const universally_unique_identifier& uuid);
};
