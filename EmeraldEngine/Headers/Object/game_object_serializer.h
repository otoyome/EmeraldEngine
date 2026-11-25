#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class scene_system;

//	game_objectのシリアライザー
class game_object_serializer
{
public:
	//	オブジェクト以外の項目を同じ JSON に書くための関数
	using write_function = std::function<void(cereal::JSONOutputArchive&)>;

	//	オブジェクト以外の項目を同じ JSON から読むための関数（そのファイルのオブジェクトを読んだ後に呼ぶ）
	using read_function = std::function<void(cereal::JSONInputArchive&)>;

	//	ファイルの外のオブジェクトを UUID から引く関数
	using find_function = std::function<game_object_ptr(const core::uuid&)>;

public:
	//	保存
	static bool save(std::ostream& stream, const std::vector<game_object_ptr>& objects, const write_function& write_extra = {});

	//	読込
	static bool load(std::istream& stream, std::vector<game_object_ptr>& out_objects, const read_function& read_extra = {});

	//	オブジェクトの中身を保存する
	static bool save_snapshot(std::ostream& stream, const game_object& object);

	//	登録済みのオブジェクトに save_snapshot の中身を読み直す
	static bool load_snapshot(std::istream& stream, game_object& object, const find_function& find);

	//	シーンシステムの中身を保存する
	static bool save_snapshot(std::ostream& stream, const scene_system& system);

	//	シーンシステムに save_snapshot の中身を読み直す
	static bool load_snapshot(std::istream& stream, scene_system& system, const find_function& find);

public:
	//	アセットへの参照を UUID 文字列として読み書きする
	static void save_asset_reference(cereal::JSONOutputArchive& archive, const char* name, const core::uuid& uuid);
	static core::uuid load_asset_reference(cereal::JSONInputArchive& archive, const char* name);
	static void save_asset_reference(cereal::JSONOutputArchive& archive, const core::uuid& uuid);
	static core::uuid load_asset_reference(cereal::JSONInputArchive& archive);

	//	UUID からパスを引く
	static std::string resolve_asset_path(const core::uuid& uuid);

	//	アセットへの参照シリアライズ
	static void serialize_asset_reference(cereal::JSONOutputArchive& archive, const char* name, core::uuid& uuid) { save_asset_reference(archive, name, uuid); }
	static void serialize_asset_reference(cereal::JSONInputArchive& archive, const char* name, core::uuid& uuid) { uuid = load_asset_reference(archive, name); }

	//	クラスの保存項目を name の下に { "version", 項目... } として読み書きする
	template<class Archive, class Owner>
	static void save_fields(Archive& archive, const char* name, const Owner& owner)
	{
		std::uint32_t version = Owner::serialize_version;
		archive.setNextName(name);
		archive.startNode();
		archive.setNextName("version");
		archive(version);
		const_cast<Owner&>(owner).serialize_fields(archive, version);
		archive.finishNode();
	}

	template<class Archive, class Owner>
	static void load_fields(Archive& archive, const char* name, Owner& owner)
	{
		const char* next_name = archive.getNodeName();
		if (!next_name || std::string_view(next_name) != name)
		{
			owner.serialize_fields(archive, 0);
			return;
		}

		std::uint32_t version = 0;
		archive.setNextName(name);
		archive.startNode();
		archive.setNextName("version");
		archive(version);
		owner.serialize_fields(archive, version);
		archive.finishNode();
	}

	//	同じファイル内の参照を UUID 文字列として保存
	template<class Type>
	static void save_object_reference(cereal::JSONOutputArchive& archive, const char* name, const std::shared_ptr<Type>& object)
	{
		save_object(archive, name, object);
	}
	template<class Type>
	static void load_object_reference(cereal::JSONInputArchive& archive, const char* name, std::shared_ptr<Type>& out_object)
	{
		out_object = std::dynamic_pointer_cast<Type>(load_object(archive, name));
	}

	//	同じファイル内の参照シリアライズ
	template<class Type>
	static void serialize_object_reference(cereal::JSONOutputArchive& archive, const char* name, std::shared_ptr<Type>& object) { save_object_reference(archive, name, object); }
	template<class Type>
	static void serialize_object_reference(cereal::JSONInputArchive& archive, const char* name, std::shared_ptr<Type>& object) { load_object_reference(archive, name, object); }

private:
	static void save_object(cereal::JSONOutputArchive& archive, const char* name, const game_object_ptr& object);
	static game_object_ptr load_object(cereal::JSONInputArchive& archive, const char* name);
	static game_object_ptr find_object(cereal::JSONInputArchive& archive, const core::uuid& uuid);
};
