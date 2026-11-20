#include "Resource/asset_registry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

#include "Resource/resource_base.h"
#include "Resource/resource_manager.h"
#include "system/logger.h"

asset_registry::asset_registry()
{
}

asset_registry::~asset_registry()
{
	save_table();
}

void asset_registry::scan(std::string_view directory)
{
	scan(std::vector<std::string_view>{ directory });
}

void asset_registry::scan(const std::vector<std::string_view>& directories)
{
	std::unique_lock lock(mutex);
	entries.clear();
	path_to_uuid.clear();

	//	ヘッダー収集
	std::vector<found_asset> assets;
	for (std::string_view directory : directories)
		collect_assets(directory, assets);

	//	同じ UUID が複数あれば、前回の表と同じパスのものを残す
	const std::unordered_map<universally_unique_identifier, std::wstring> previous = load_previous_table();
	std::unordered_map<universally_unique_identifier, size_t> keepers;
	for (size_t index = 0; index < assets.size(); ++index)
	{
		auto [it, inserted] = keepers.try_emplace(assets[index].uuid, index);
		if (inserted)
			continue;
		auto previous_it = previous.find(assets[index].uuid);
		if (previous_it != previous.end() && previous_it->second == resource_manager::make_path_key(assets[index].path))
			it->second = index;
	}

	//	登録処理
	size_t duplicated_count = 0;
	size_t moved_count = 0;
	for (size_t index = 0; index < assets.size(); ++index)
	{
		found_asset& asset = assets[index];
		if (keepers[asset.uuid] == index)
		{
			//	前回と違う場所にあれば外で移動された
			auto previous_it = previous.find(asset.uuid);
			if (previous_it != previous.end() && previous_it->second != resource_manager::make_path_key(asset.path))
			{
				DEBUG_LOG_CATEGORY(logger::categorys::system, "asset_registry moved[%s]", asset.path.c_str());
				++moved_count;
			}
			register_asset_core(asset.uuid, asset.type, asset.path);
			continue;
		}

		//	UUID を振り直す
		const universally_unique_identifier new_uuid = universally_unique_identifier::generate();
		if (!resource::rewrite_asset_header_uuid(std::filesystem::path(asset.path), new_uuid))
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "asset_registry uuid rewrite failed[%s]", asset.path.c_str());
			continue;
		}
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "asset_registry duplicated uuid[%s] -> %s", asset.path.c_str(), new_uuid.to_string().c_str());
		register_asset_core(new_uuid, asset.type, asset.path);
		++duplicated_count;
	}

	DEBUG_LOG_CATEGORY(logger::categorys::system, "asset_registry scan assets:%zu duplicated:%zu moved:%zu", entries.size(), duplicated_count, moved_count);
	save_table_locked();
}

void asset_registry::collect_assets(std::string_view directory, std::vector<found_asset>& out_assets)
{
	std::error_code error;
	const std::filesystem::path root{ std::string(directory) };
	if (!std::filesystem::is_directory(root, error))
		return;

	for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, error), end; it != end; it.increment(error))
	{
		if (error)
			break;
		if (!it->is_regular_file(error))
			continue;

		//	リソースが管轄している物か判断
		if (resource::get_asset_type_from_extension(it->path().extension().string()) == resource::asset_types::unknown)
			continue;

		//	ヘッダーを読み込む
		resource::asset_file_header header;
		if (!resource::read_asset_header(it->path(), header) || header.uuid.is_empty())
			continue;

		out_assets.push_back({ header.uuid, header.type, it->path().generic_string() });
	}
}

std::unordered_map<universally_unique_identifier, std::wstring> asset_registry::load_previous_table()
{
	//	1行に「UUID<TAB>パス」の形式で保存
	std::unordered_map<universally_unique_identifier, std::wstring> table;
	std::ifstream stream(std::string(previous_table_path), std::ios::binary);
	std::string line;
	while (std::getline(stream, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		const size_t tab = line.find('\t');
		if (tab == std::string::npos)
			continue;
		universally_unique_identifier uuid;
		if (!universally_unique_identifier::from_string(line.substr(0, tab), uuid))
			continue;
		table.insert_or_assign(uuid, resource_manager::make_path_key(std::string_view(line).substr(tab + 1)));
	}
	return table;
}

void asset_registry::save_table() const
{
	std::shared_lock lock(mutex);
	save_table_locked();
}

void asset_registry::save_table_locked() const
{
	if (entries.empty())
		return;

	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(std::string(previous_table_path)).parent_path(), error);

	std::vector<std::pair<std::string, std::string>> lines;
	lines.reserve(entries.size());
	for (const auto& [uuid, value] : entries)
		lines.emplace_back(value.path, uuid.to_string());
	std::sort(lines.begin(), lines.end());

	std::ofstream stream(std::string(previous_table_path), std::ios::binary | std::ios::trunc);
	if (!stream)
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "asset_registry table save failed[%s]", std::string(previous_table_path).c_str());
		return;
	}
	for (const auto& [path, uuid] : lines)
		stream << uuid << '\t' << path << "\r\n";
}

void asset_registry::register_asset(const universally_unique_identifier& uuid, resource::asset_types type, std::string_view path)
{
	if (uuid.is_empty() || path.empty())
		return;

	std::unique_lock lock(mutex);
	register_asset_core(uuid, type, path);
}

void asset_registry::unregister_path(std::string_view path)
{
	std::unique_lock lock(mutex);
	unregister_asset(resource_manager::make_path_key(path));
}

void asset_registry::move_path(std::string_view old_path, std::string_view new_path)
{
	std::unique_lock lock(mutex);

	std::vector<std::pair<universally_unique_identifier, std::string>> moved_entries;
	for (const auto& [uuid, value] : entries)
	{
		std::string moved_path;
		if (resource_manager::replace_base_path(value.path, old_path, new_path, moved_path))
			moved_entries.emplace_back(uuid, std::move(moved_path));
	}
	for (const auto& [uuid, moved_path] : moved_entries)
		register_asset_core(uuid, entries[uuid].type, moved_path);
}

universally_unique_identifier asset_registry::find_uuid(std::string_view path) const
{
	const std::wstring key = resource_manager::make_path_key(path);

	std::shared_lock lock(mutex);
	const auto it = path_to_uuid.find(key);
	return it != path_to_uuid.end() ? it->second : universally_unique_identifier{};
}

std::string asset_registry::find_path(const universally_unique_identifier& uuid) const
{
	if (uuid.is_empty())
		return {};

	std::shared_lock lock(mutex);
	const auto it = entries.find(uuid);
	return it != entries.end() ? it->second.path : std::string{};
}

void asset_registry::prepare_save(resource::resource_base& target, std::string_view path) const
{
	const std::string registered_path = find_path(target.get_uuid());
	if (registered_path.empty() || resource_manager::make_path_key(registered_path) == resource_manager::make_path_key(path))
		return;

	//	元のファイルが無ければ移動とみなして UUID はそのまま
	std::error_code error;
	if (!std::filesystem::exists(std::filesystem::path(registered_path), error))
		return;

	target.regenerate_uuid();
}

std::vector<std::string> asset_registry::find_paths(resource::asset_types type) const
{
	std::vector<std::string> paths;
	{
		std::shared_lock lock(mutex);
		for (const auto& [uuid, value] : entries)
		{
			if (value.type == type)
				paths.emplace_back(value.path);
		}
	}
	std::sort(paths.begin(), paths.end());
	return paths;
}

size_t asset_registry::get_count() const
{
	std::shared_lock lock(mutex);
	return entries.size();
}

void asset_registry::register_asset_core(const universally_unique_identifier& uuid, resource::asset_types type, std::string_view path)
{
	const std::wstring key = resource_manager::make_path_key(path);

	//	同じパスに別の UUID が載っていれば(別のアセットで上書きされた)外す
	if (const auto path_it = path_to_uuid.find(key); path_it != path_to_uuid.end() && path_it->second != uuid)
		unregister_asset(key);

	//	同じ UUID が別のパスに載っていれば(移動)古いパスを外す
	if (const auto entry_it = entries.find(uuid); entry_it != entries.end())
	{
		const std::wstring old_key = resource_manager::make_path_key(entry_it->second.path);
		if (old_key != key)
			path_to_uuid.erase(old_key);
	}

	entries.insert_or_assign(uuid, entry{ type, std::filesystem::path(std::string(path)).lexically_normal().generic_string() });
	path_to_uuid.insert_or_assign(key, uuid);
}

void asset_registry::unregister_asset(const std::wstring& key)
{
	const auto it = path_to_uuid.find(key);
	if (it == path_to_uuid.end())
		return;

	entries.erase(it->second);
	path_to_uuid.erase(it);
}

#if defined(USE_IMGUI)
void asset_registry::imgui_render()
{
	filter.Draw("filter");

	std::shared_lock lock(mutex);
	ImGui::Text("assets : %zu", entries.size());
	for (const auto& [uuid, value] : entries)
	{
		const std::string utf8_path(system_utility::convert_utf8(value.path));
		if (!filter.PassFilter(utf8_path.c_str()))
			continue;
		ImGui::Text("%s  %s  %s", uuid.to_string().c_str(), magic_enum::enum_name(value.type).data(), utf8_path.c_str());
	}
}
#endif	//	defined(USE_IMGUI)
