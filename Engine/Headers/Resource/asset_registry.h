#pragma once

#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
	//	アプリケーション用のアセット
	static constexpr std::string_view root_directory = "Assets";
	//	エンジン・エディター用のアセット
	static constexpr std::string_view engine_root_directory = "EngineAssets";

	struct entry
	{
		resource::asset_types type = resource::asset_types::unknown;
		std::string path;	//	相対パス
	};

private:
	asset_registry();
	~asset_registry();
	asset_registry(const asset_registry&) = delete;
	asset_registry(asset_registry&&) = delete;

public:
	//	フォルダ以下のアセットをスキャン
	void scan(std::string_view directory = root_directory);
	void scan(const std::vector<std::string_view>& directories);

	//	アセット登録
	void register_asset(const universally_unique_identifier& uuid, resource::asset_types type, std::string_view path);
	void unregister_path(std::string_view path);
	void move_path(std::string_view old_path, std::string_view new_path);

	//	検索
	universally_unique_identifier find_uuid(std::string_view path) const;
	std::string find_path(const universally_unique_identifier& uuid) const;
	std::vector<std::string> find_paths(resource::asset_types type) const;

	//	UUIDから検索して見つからないならヒントパスを渡す
	std::string resolve_path(const universally_unique_identifier& uuid, std::string_view hint_path) const;

	//	保存の前に呼ぶ
	void prepare_save(resource::resource_base& target, std::string_view path) const;

	//	アセット数取得
	size_t get_count() const;

#if defined(USE_IMGUI)
	void imgui_render();
#endif	//	defined(USE_IMGUI)

private:
	void scan_asset(std::string_view directory, size_t& duplicated_count);
	void register_asset_core(const universally_unique_identifier& uuid, resource::asset_types type, std::string_view path);
	void unregister_asset(const std::wstring& key);

private:
	mutable std::shared_mutex mutex;
	std::unordered_map<universally_unique_identifier, entry> entries;
	std::unordered_map<std::wstring, universally_unique_identifier> path_to_uuid;	//	キーは resource_manager::make_path_key

#if defined(USE_IMGUI)
	ImGuiTextFilter filter;
#endif	//	defined(USE_IMGUI)
};
