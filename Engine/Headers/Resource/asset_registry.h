#pragma once

#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "system/singleton.h"

#include "Resource/asset_file_header.h"
#include "Resource/asset_reference.h"

namespace resource
{
	class resource_base;
}	//	namespace resource

//	アセットレジスター
class asset_registry
{
	SINGLETON(asset_registry)

public:
	//	アプリケーション用のアセット
	static constexpr std::string_view root_directory = "Assets";
	//	エンジン・エディター用のアセット
	static constexpr std::string_view engine_root_directory = "EngineAssets";
	//	前回の対応表
	static constexpr std::string_view previous_table_path = "Intermediate/asset_registry.txt";

	//	スキャン時に見つかったアセット
	struct found_asset
	{
		core::uuid uuid;
		resource::asset_types type = resource::asset_types::unknown;
		std::string path;
	};

	//	アセット
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
	void register_asset(const core::uuid& uuid, resource::asset_types type, std::string_view path);
	void unregister_path(std::string_view path);
	void move_path(std::string_view old_path, std::string_view new_path);

	//	検索
	core::uuid find_uuid(std::string_view path) const;
	std::string find_path(const core::uuid& uuid) const;

	//	UUIDからパスを検索
	template<class Type>
	std::string find_path(const resource::asset_reference<Type>& reference) const { return find_path(reference.uuid); }
	std::vector<std::string> find_paths(resource::asset_types type) const;

	//	保存の前に呼ぶ
	void prepare_save(resource::resource_base& target, std::string_view path) const;

	//	アセット数取得
	size_t get_count() const;

	//	対応表を previous_table_path に保存
	void save_table() const;

#if defined(USE_IMGUI)
	void imgui_render();
#endif	//	defined(USE_IMGUI)

private:

	static void collect_assets(std::string_view directory, std::vector<found_asset>& out_assets);
	static std::unordered_map<core::uuid, std::wstring> load_previous_table();
	void save_table_locked() const;
	void register_asset_core(const core::uuid& uuid, resource::asset_types type, std::string_view path);
	void unregister_asset(const std::wstring& key);

private:
	mutable std::shared_mutex mutex;
	std::unordered_map<core::uuid, entry> entries;
	std::unordered_map<std::wstring, core::uuid> path_to_uuid;	//	キーは resource_manager::make_path_key

#if defined(USE_IMGUI)
	ImGuiTextFilter filter;
#endif	//	defined(USE_IMGUI)
};
