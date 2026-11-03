#pragma once

#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "system/singleton.h"

#include "Resource/asset_file_header.h"

namespace resource
{
	class resource_base;
}	//	namespace resource

//	UUID とアセットファイルの対応表
class asset_registry
{
	SINGLETON(asset_registry)

public:
	static constexpr std::string_view root_directory = "Assets";

	struct entry
	{
		resource::asset_types type = resource::asset_types::unknown;
		std::string path;	//	「/」区切りの相対パス(Shift-JIS)
	};

private:
	asset_registry();
	~asset_registry();
	asset_registry(const asset_registry&) = delete;
	asset_registry(asset_registry&&) = delete;

public:
	//	フォルダ以下のヘッダーを読んで対応表を作り直す
	void scan(std::string_view directory = root_directory);

	//	アセット登録
	void register_asset(const universally_unique_identifier& uuid, resource::asset_types type, std::string_view path);
	void unregister_path(std::string_view path);
	void move_path(std::string_view old_path, std::string_view new_path);

	//	以下はワーカースレッドからも呼ばれる
	universally_unique_identifier find_uuid(std::string_view path) const;
	std::string find_path(const universally_unique_identifier& uuid) const;

	//	UUID からパスを引く。引けなければ控えのパスを返す
	std::string resolve_path(const universally_unique_identifier& uuid, std::string_view hint_path) const;

	//	保存の前に呼ぶ。同じ UUID の別のファイルが残っていれば(名前を付けて保存)、新しい UUID にする
	void prepare_save(resource::resource_base& target, std::string_view path) const;

	size_t get_count() const;

#if defined(USE_IMGUI)
	void imgui_render();
#endif	//	defined(USE_IMGUI)

private:
	void register_locked(const universally_unique_identifier& uuid, resource::asset_types type, std::string_view path);
	void unregister_locked(const std::wstring& key);

private:
	mutable std::shared_mutex mutex;
	std::unordered_map<universally_unique_identifier, entry> entries;
	std::unordered_map<std::wstring, universally_unique_identifier> path_to_uuid;	//	キーは resource_manager::make_path_key

#if defined(USE_IMGUI)
	ImGuiTextFilter filter;
#endif	//	defined(USE_IMGUI)
};
