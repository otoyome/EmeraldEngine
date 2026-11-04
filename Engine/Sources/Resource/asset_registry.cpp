#include "Resource/asset_registry.h"

#include <filesystem>
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
}

void asset_registry::scan(std::string_view directory)
{
	std::unique_lock lock(mutex);
	entries.clear();
	path_to_uuid.clear();

	std::error_code error;
	const std::filesystem::path root{ std::string(directory) };
	if (!std::filesystem::is_directory(root, error))
		return;

	size_t duplicated_count = 0;
	for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, error), end; it != end; it.increment(error))
	{
		if (error)
			break;
		if (!it->is_regular_file(error))
			continue;
		if (resource::get_asset_type_from_extension(it->path().extension().string()) == resource::asset_types::unknown)
			continue;

		resource::asset_file_header header;
		if (!resource::read_asset_header(it->path(), header) || header.uuid.is_empty())
			continue;

		const std::string path = it->path().generic_string();
		if (entries.contains(header.uuid))
		{
			const universally_unique_identifier new_uuid = universally_unique_identifier::generate();
			if (!resource::rewrite_asset_header_uuid(it->path(), new_uuid))
			{
				DEBUG_ERROR_CATEGORY(logger::categorys::system, "asset_registry uuid rewrite failed[%s]", path.c_str());
				continue;
			}
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "asset_registry duplicated uuid[%s] -> %s", path.c_str(), new_uuid.to_string().c_str());
			header.uuid = new_uuid;
			++duplicated_count;
		}
		register_locked(header.uuid, header.type, path);
	}

	DEBUG_LOG_CATEGORY(logger::categorys::system, "asset_registry scan[%s] assets:%zu duplicated:%zu", root.generic_string().c_str(), entries.size(), duplicated_count);
}

void asset_registry::register_asset(const universally_unique_identifier& uuid, resource::asset_types type, std::string_view path)
{
	if (uuid.is_empty() || path.empty())
		return;

	std::unique_lock lock(mutex);
	register_locked(uuid, type, path);
}

void asset_registry::unregister_path(std::string_view path)
{
	std::unique_lock lock(mutex);
	unregister_locked(resource_manager::make_path_key(path));
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
		register_locked(uuid, entries[uuid].type, moved_path);
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

std::string asset_registry::resolve_path(const universally_unique_identifier& uuid, std::string_view hint_path) const
{
	std::string path = find_path(uuid);
	if (path.empty())
		path = hint_path;
	return path;
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

size_t asset_registry::get_count() const
{
	std::shared_lock lock(mutex);
	return entries.size();
}

void asset_registry::register_locked(const universally_unique_identifier& uuid, resource::asset_types type, std::string_view path)
{
	const std::wstring key = resource_manager::make_path_key(path);

	//	同じパスに別の UUID が載っていれば(別のアセットで上書きされた)外す
	if (const auto path_it = path_to_uuid.find(key); path_it != path_to_uuid.end() && path_it->second != uuid)
		unregister_locked(key);

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

void asset_registry::unregister_locked(const std::wstring& key)
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
