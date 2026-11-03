#include "System/asset_browser.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <shellapi.h>

#include "System/console_manager.h"
#include "texture.h"
#include "Resource/resource_manager.h"
#include "Resource/asset_registry.h"
#include "system/logger.h"

namespace
{
	//	一覧の起点
	constexpr std::string_view root_directory = "Assets";

	//	@tool ドラッグ&ドロップの種類名
	constexpr const char* move_payload_type = "ASSET_BROWSER_PATH";

	//	@tool 本体のファイル名の後ろに付けた名前の付属ファイル(本体と一緒に移動する)
	constexpr std::string_view sidecar_extensions[] = { ".meta" };

	//	一覧に出さないフォルダ
	constexpr std::string_view excluded_directories[] = { "Assets/CSO" };

	//	サムネイルを読み込む拡張子
	constexpr std::string_view thumbnail_extensions[] = { ".png", ".dds", ".tga", ".bmp", ".jpg", ".jpeg", ".hdr" };

	//	一覧に出さない拡張子
	constexpr std::string_view hidden_extensions[] = { ".meta" };

	//	1フレームでサムネイルの読み込みに使う時間
	constexpr std::chrono::milliseconds thumbnail_load_budget(4);

	//	描画が終わるまでサムネイルを保持するフレーム数
	constexpr std::uint64_t thumbnail_keep_frames = 4;

	console_variable<float> thumbnail_size_variable("editor.asset_browser.thumbnail_size", 128.0f, "アセットブラウザのサムネイルの大きさ", console_flags::persist);

	std::string to_lower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	template<typename List>
	bool contains(const List& list, std::string_view value)
	{
		return std::find_if(std::begin(list), std::end(list), [&](const auto& item) { return std::string_view(item) == value; }) != std::end(list);
	}

	std::filesystem::path to_filesystem_path(const cache_string& path)
	{
		return std::filesystem::path(path.get<utf16_cache>());
	}

	//	名前順の比較（大文字・小文字は区別しない）
	bool less_name(const cache_string& l, const cache_string& r)
	{
		return to_lower(l.get<utf8_cache>()) < to_lower(r.get<utf8_cache>());
	}

	std::string format_size(std::uintmax_t size)
	{
		char text[64] = {};
		if (size < 1024)
			sprintf_s(text, "%llu B", static_cast<unsigned long long>(size));
		else if (size < 1024 * 1024)
			sprintf_s(text, "%.1f KB", static_cast<double>(size) / 1024.0);
		else
			sprintf_s(text, "%.1f MB", static_cast<double>(size) / (1024.0 * 1024.0));
		return text;
	}

	//	種類ごとのアイコンの色
	ImU32 extension_color(std::string_view extension)
	{
		constexpr std::string_view model_extensions[] = { ".emo", ".ebmo", ".ebmc", ".pmx", ".gltf", ".glb", ".mqo", ".mqoz" };
		constexpr std::string_view sound_extensions[] = { ".wav", ".ogg", ".mp3" };
		constexpr std::string_view animation_extensions[] = { ".vmd", ".pmm" };

		if (contains(model_extensions, extension))
			return IM_COL32(80, 130, 210, 255);
		if (extension == ".eme")
			return IM_COL32(220, 130, 50, 255);
		if (extension == ".etl")
			return IM_COL32(150, 100, 200, 255);
		if (extension == ".emr")
			return IM_COL32(70, 170, 100, 255);
		if (contains(sound_extensions, extension))
			return IM_COL32(50, 170, 170, 255);
		if (contains(animation_extensions, extension))
			return IM_COL32(160, 170, 60, 255);
		if (contains(thumbnail_extensions, extension))
			return IM_COL32(190, 90, 130, 255);
		return IM_COL32(110, 110, 110, 255);
	}

	//	path が target の親フォルダか
	bool is_ancestor(std::string_view path, std::string_view target)
	{
		return target.size() > path.size() && target.substr(0, path.size()) == path && target[path.size()] == '/';
	}
}

asset_browser::asset_browser()
{
	current_directory = root_directory;
	refresh();
}

asset_browser::~asset_browser()
{
}

void asset_browser::register_open_handler(open_handler handler)
{
	for (auto& extension : handler.extensions)
		extension = to_lower(extension.str());
	handlers.emplace_back(std::move(handler));
}

bool asset_browser::take_open_request(std::string_view name, std::string& out_file_path)
{
	auto it = open_requests.find(name);
	if (it == open_requests.end())
		return false;

	//	@tool 要求してから受け取るまでに移動されていても、UUID から今のパスを引く
	const open_request request = std::move(it->second);
	open_requests.erase(it);
	out_file_path = asset_registry::is_valid() ? asset_registry::instance()->resolve_path(request.uuid, request.file_path) : request.file_path;
	return true;
}

bool asset_browser::open_asset(const universally_unique_identifier& uuid)
{
	const std::string file_path = asset_registry::is_valid() ? asset_registry::instance()->find_path(uuid) : std::string{};
	if (file_path.empty())
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "asset_browser asset not found[%s]", uuid.to_string().c_str());
		return false;
	}

	const auto handler = find_default_handler(to_lower(system_utility::convert_utf16_to_shift_jis(to_filesystem_path(file_path).extension().wstring())));
	if (!handler)
		return false;

	request_open(*handler, file_path);
	return true;
}

void asset_browser::request_open(const open_handler& handler, const cache_string& file_path)
{
	const universally_unique_identifier uuid = asset_registry::is_valid() ? asset_registry::instance()->find_uuid(file_path.view()) : universally_unique_identifier{};
	open_requests.insert_or_assign(handler.name.str(), open_request{ uuid, file_path.str() });
	if (handler.activate)
		handler.activate();
}

const asset_browser::open_handler* asset_browser::find_default_handler(std::string_view extension) const
{
	for (auto& handler : handlers)
	{
		if (contains(handler.extensions, extension))
			return &handler;
	}
	return nullptr;
}

void asset_browser::refresh()
{
	root = {};
	root.name = root_directory;
	root.path = root_directory;
	build_directory_tree(root);

	//	消えたフォルダを見ていたら起点に戻す
	std::error_code error;
	if (!std::filesystem::is_directory(to_filesystem_path(current_directory), error))
		current_directory = root_directory;

	scan_entries();
}

void asset_browser::build_directory_tree(directory_node& node)
{
	std::error_code error;
	std::filesystem::directory_iterator it(to_filesystem_path(node.path), error);
	for (; !error && it != std::filesystem::directory_iterator(); it.increment(error))
	{
		std::error_code type_error;
		if (!it->is_directory(type_error))
			continue;

		const std::wstring name = it->path().filename().wstring();
		const std::string sjis_name = system_utility::convert_utf16_to_shift_jis(name);
		cache_string path = node.path.str() + "/" + sjis_name;
		if (contains(excluded_directories, path.view()))
			continue;

		auto& child = node.children.emplace_back();
		child.name = sjis_name;
		child.path = std::move(path);
	}

	std::sort(node.children.begin(), node.children.end(),
		[](const directory_node& l, const directory_node& r)
		{
			return less_name(l.name, r.name);
		});

	for (auto& child : node.children)
		build_directory_tree(child);
}

void asset_browser::change_directory(const cache_string& path)
{
	if (path == current_directory)
		return;

	current_directory = path;
	selected_path.clear();
	filter.Clear();
	reveal_current_directory = true;
	scan_entries();
}

void asset_browser::scan_entries()
{
	retire_thumbnails();
	entries.clear();

	std::error_code error;
	std::filesystem::directory_iterator it(to_filesystem_path(current_directory), error);
	for (; !error && it != std::filesystem::directory_iterator(); it.increment(error))
	{
		std::error_code type_error;
		const bool is_directory = it->is_directory(type_error);
		const std::wstring name = it->path().filename().wstring();

		entry target;
		target.name = system_utility::convert_utf16_to_shift_jis(name);
		target.path = current_directory.str() + "/" + target.name.str();
		target.is_directory = is_directory;
		if (is_directory && contains(excluded_directories, target.path.view()))
			continue;
		if (!is_directory)
		{
			target.extension = to_lower(system_utility::convert_utf16_to_shift_jis(it->path().extension().wstring()));
			if (contains(hidden_extensions, target.extension))
				continue;

			std::error_code size_error;
			target.file_size = it->file_size(size_error);
		}
		entries.emplace_back(std::move(target));
	}

	//	フォルダを先に、名前順に並べる
	std::sort(entries.begin(), entries.end(),
		[](const entry& l, const entry& r)
		{
			if (l.is_directory != r.is_directory)
				return l.is_directory;
			return less_name(l.name, r.name);
		});
}

void asset_browser::retire_thumbnails()
{
	for (auto& target : entries)
	{
		if (target.thumbnail)
			retired_thumbnails.push_back({ std::move(target.thumbnail), frame_index });
	}
}

void asset_browser::release_retired_thumbnails()
{
	retired_thumbnails.erase(
		std::remove_if(retired_thumbnails.begin(), retired_thumbnails.end(),
			[this](const retired_thumbnail& retired)
			{
				return frame_index - retired.frame >= thumbnail_keep_frames;
			}),
		retired_thumbnails.end());
}

void asset_browser::load_thumbnail(entry& target)
{
	target.thumbnail_requested = true;
	if (!contains(thumbnail_extensions, target.extension))
		return;

	texture_ptr loaded;
	if (!texture_manager::instance()->load_texture_from_file(loaded, target.path) || !loaded)
		return;

	if (loaded->get_dimension() != D3D11_RESOURCE_DIMENSION_TEXTURE2D ||
		!loaded->get_shader_resource_view() ||
		loaded->is_flag(texture::flags::cube_map))
		return;

	//	整数フォーマットは ImGui で表示できない
	if (graphics_utility::is_int_format(static_cast<texture2d*>(loaded.get())->get_desc().Format))
		return;

	target.thumbnail = loaded;
}

void asset_browser::imgui_render()
{
	++frame_index;
	release_retired_thumbnails();

	ImGui::SetNextWindowSize(ImVec2(1200.0f, 700.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Asset Browser"))
	{
		imgui_render_toolbar();
		ImGui::Separator();

		if (ImGui::BeginTable("##asset_browser_layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("##tree", ImGuiTableColumnFlags_WidthFixed, 240.0f);
			ImGui::TableSetupColumn("##entries", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			if (ImGui::BeginChild("##tree"))
			{
				imgui_render_directory_tree(root);
				reveal_current_directory = false;
			}
			ImGui::EndChild();

			ImGui::TableSetColumnIndex(1);
			if (ImGui::BeginChild("##entries"))
			{
				imgui_render_entries();
			}
			ImGui::EndChild();

			ImGui::EndTable();
		}

		imgui_render_rename_popup();
	}
	ImGui::End();

	if (pending_move)
	{
		move_entry(pending_move->source_path, pending_move->destination_path);
		pending_move.reset();
	}

	if (pending_refresh)
	{
		pending_refresh = false;
		pending_directory.clear();
		refresh();
	}
	else if (!pending_directory.empty())
	{
		change_directory(pending_directory);
		pending_directory.clear();
	}
}

void asset_browser::imgui_render_toolbar()
{
	if (ImGui::Button("Refresh"))
		pending_refresh = true;

	//	一つ上のフォルダへ
	ImGui::SameLine();
	const std::string& directory = current_directory.str();
	ImGui::BeginDisabled(current_directory.view() == root_directory);
	if (ImGui::ArrowButton("##up", ImGuiDir_Up))
		pending_directory = directory.substr(0, directory.rfind('/'));
	ImGui::EndDisabled();

	//	現在のフォルダまでの経路
	{
		std::size_t begin = 0;
		int index = 0;
		while (begin <= directory.size())
		{
			std::size_t end = directory.find('/', begin);
			if (end == std::string::npos)
				end = directory.size();

			ImGui::SameLine();
			if (index > 0)
			{
				ImGui::TextUnformatted(">");
				ImGui::SameLine();
			}
			ImGui::PushID(index++);
			const std::string name = system_utility::convert_shift_jis_to_utf8(std::string_view(directory).substr(begin, end - begin));
			if (ImGui::SmallButton(name.c_str()))
				pending_directory = directory.substr(0, end);
			ImGui::PopID();

			begin = end + 1;
		}
	}

	//	絞り込みとサムネイルの大きさは右端に置く
	const ImGuiStyle& style = ImGui::GetStyle();
	const float filter_width = 240.0f;
	const float size_width = 160.0f;
	const float right_x = ImGui::GetWindowContentRegionMax().x - filter_width - size_width - style.ItemSpacing.x;
	ImGui::SameLine();
	if (ImGui::GetCursorPosX() < right_x)
		ImGui::SetCursorPosX(right_x);

	ImGui::SetNextItemWidth(filter_width);
	if (ImGui::InputTextWithHint("##filter", "Filter", filter.InputBuf, IM_ARRAYSIZE(filter.InputBuf)))
		filter.Build();

	ImGui::SameLine();
	ImGui::SetNextItemWidth(size_width);
	ImGui::SliderFloat("##thumbnail_size", &thumbnail_size_variable.ref(), 48.0f, 256.0f, "%.0f");
}

void asset_browser::imgui_render_directory_tree(const directory_node& node)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (node.children.empty())
		flags |= ImGuiTreeNodeFlags_Leaf;
	if (node.path == current_directory)
		flags |= ImGuiTreeNodeFlags_Selected;
	if (node.path.view() == root_directory)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	//	移動した先のフォルダまでを開く
	if (reveal_current_directory && is_ancestor(node.path.view(), current_directory.view()))
		ImGui::SetNextItemOpen(true);

	const bool opened = ImGui::TreeNodeEx(node.path.c_str(), flags, "%s", node.name.get<utf8_cache>().c_str());
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		pending_directory = node.path;
	accept_move_drop(node.path);

	if (opened)
	{
		for (auto& child : node.children)
			imgui_render_directory_tree(child);
		ImGui::TreePop();
	}
}

void asset_browser::imgui_render_entries()
{
	std::vector<entry*> visible_entries;
	for (auto& target : entries)
	{
		if (filter.PassFilter(target.name.get<utf8_cache>().c_str()))
			visible_entries.emplace_back(&target);
	}
	if (visible_entries.empty())
	{
		ImGui::TextDisabled("Empty");
		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const float thumbnail_size = thumbnail_size_variable.get();
	const float cell_width = thumbnail_size + style.FramePadding.x * 2.0f;
	const float cell_height = thumbnail_size + style.FramePadding.y * 3.0f + ImGui::GetTextLineHeight() * 2.0f;
	const float region_width = ImGui::GetContentRegionAvail().x;
	const int columns = std::max(1, static_cast<int>((region_width + style.ItemSpacing.x) / (cell_width + style.ItemSpacing.x)));
	const int rows = static_cast<int>((visible_entries.size() + columns - 1) / columns);

	thumbnail_load_deadline = std::chrono::steady_clock::now() + thumbnail_load_budget;

	//	見えている行だけを処理
	ImGuiListClipper clipper;
	clipper.Begin(rows, cell_height + style.ItemSpacing.y);
	while (clipper.Step())
	{
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
		{
			for (int column = 0; column < columns; ++column)
			{
				const std::size_t index = static_cast<std::size_t>(row) * columns + column;
				if (index >= visible_entries.size())
					break;

				if (column > 0)
					ImGui::SameLine();
				imgui_render_entry(*visible_entries[index], cell_width, cell_height);
			}
		}
	}
	clipper.End();

	//	何も無い所をクリックしたら選択を外す
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
		selected_path.clear();
}

void asset_browser::imgui_render_entry(entry& target, float cell_width, float cell_height)
{
	ImGui::PushID(target.path.c_str());

	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 cell_min = ImGui::GetCursorScreenPos();
	const ImVec2 cell_max(cell_min.x + cell_width, cell_min.y + cell_height);

	if (ImGui::Selectable("##cell", target.path == selected_path, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(cell_width, cell_height)))
	{
		selected_path = target.path;
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (target.is_directory)
			{
				pending_directory = target.path;
			}
			else if (auto handler = find_default_handler(target.extension))
			{
				request_open(*handler, target.path);
			}
		}
	}
	const bool visible = ImGui::IsItemVisible();

	//	@tool ドラッグでフォルダへ移動
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload(move_payload_type, target.path.c_str(), target.path.str().size() + 1);
		ImGui::TextUnformatted(target.name.get<utf8_cache>().c_str());
		ImGui::EndDragDropSource();
	}
	if (target.is_directory)
		accept_move_drop(target.path);

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && ImGui::BeginTooltip())
	{
		ImGui::TextUnformatted(target.name.get<utf8_cache>().c_str());
		if (!target.is_directory)
		{
			ImGui::TextDisabled("%s", format_size(target.file_size).c_str());
			if (target.thumbnail)
			{
				const auto& desc = static_cast<texture2d*>(target.thumbnail.get())->get_desc();
				ImGui::TextDisabled("%u x %u", desc.Width, desc.Height);
			}
			if (auto handler = find_default_handler(target.extension))
				ImGui::TextDisabled("Double-click : %s", handler->label.get<utf8_cache>().c_str());
		}
		ImGui::EndTooltip();
	}

	imgui_render_entry_context_menu(target);

	if (visible)
	{
		if (!target.is_directory && !target.thumbnail_requested && std::chrono::steady_clock::now() < thumbnail_load_deadline)
			load_thumbnail(target);

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const float size = cell_width - style.FramePadding.x * 2.0f;
		const ImVec2 image_min(cell_min.x + style.FramePadding.x, cell_min.y + style.FramePadding.y);
		const ImVec2 image_max(image_min.x + size, image_min.y + size);

		if (target.thumbnail)
		{
			//	縦横比を保って枠に収める
			const auto& desc = static_cast<texture2d*>(target.thumbnail.get())->get_desc();
			const float aspect = static_cast<float>(desc.Width) / static_cast<float>(std::max(desc.Height, 1u));
			ImVec2 extent(size, size);
			if (aspect > 1.0f)
				extent.y = size / aspect;
			else
				extent.x = size * aspect;
			const ImVec2 fit_min(image_min.x + (size - extent.x) * 0.5f, image_min.y + (size - extent.y) * 0.5f);
			draw_list->AddImage(target.thumbnail->get_shader_resource_view().Get(), fit_min, ImVec2(fit_min.x + extent.x, fit_min.y + extent.y));
		}
		else if (target.is_directory)
		{
			const ImU32 folder_color = IM_COL32(220, 180, 90, 255);
			draw_list->AddRectFilled(
				ImVec2(image_min.x + size * 0.08f, image_min.y + size * 0.16f),
				ImVec2(image_min.x + size * 0.45f, image_min.y + size * 0.32f),
				folder_color, size * 0.04f);
			draw_list->AddRectFilled(
				ImVec2(image_min.x + size * 0.08f, image_min.y + size * 0.24f),
				ImVec2(image_max.x - size * 0.08f, image_max.y - size * 0.14f),
				folder_color, size * 0.04f);
		}
		else
		{
			const ImVec2 icon_min(image_min.x + size * 0.15f, image_min.y + size * 0.06f);
			const ImVec2 icon_max(image_max.x - size * 0.15f, image_max.y - size * 0.06f);
			draw_list->AddRectFilled(icon_min, icon_max, extension_color(target.extension), size * 0.04f);

			//	拡張子を大文字で真ん中に書く
			std::string label = target.extension.empty() ? std::string("FILE") : target.extension.substr(1);
			std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
			const ImVec2 label_size = ImGui::CalcTextSize(label.c_str());
			draw_list->AddText(
				ImVec2((icon_min.x + icon_max.x - label_size.x) * 0.5f, (icon_min.y + icon_max.y - label_size.y) * 0.5f),
				IM_COL32_WHITE, label.c_str());
		}

		//	名前は2行まで折り返して、はみ出た分は切る
		const ImVec2 text_min(image_min.x, image_max.y + style.FramePadding.y);
		const ImVec4 clip_rect(text_min.x, text_min.y, cell_max.x - style.FramePadding.x, cell_max.y);
		draw_list->AddText(nullptr, 0.0f, text_min, ImGui::GetColorU32(ImGuiCol_Text), target.name.get<utf8_cache>().c_str(), nullptr, size, &clip_rect);
	}

	ImGui::PopID();
}

void asset_browser::imgui_render_entry_context_menu(const entry& target)
{
	if (!ImGui::BeginPopupContextItem("##context"))
		return;

	selected_path = target.path;

	if (target.is_directory)
	{
		if (ImGui::MenuItem("Open"))
			pending_directory = target.path;
		ImGui::Separator();
	}
	else
	{
		//	拡張子に合う開き先をすべて出す
		bool found = false;
		for (auto& handler : handlers)
		{
			if (!contains(handler.extensions, target.extension))
				continue;

			found = true;
			if (ImGui::MenuItem(handler.label.get<utf8_cache>().c_str()))
				request_open(handler, target.path);
		}
		if (found)
			ImGui::Separator();
	}

	if (ImGui::MenuItem("Rename"))
	{
		rename_source_path = target.path.str();
		strncpy_s(rename_buffer, target.name.get<utf8_cache>().c_str(), _TRUNCATE);
		rename_invalid = false;
		open_rename_popup = true;
	}

	if (ImGui::MenuItem("Copy Path"))
		ImGui::SetClipboardText(target.path.get<utf8_cache>().c_str());

	if (ImGui::MenuItem("Show in Explorer"))
	{
		std::error_code error;
		const std::wstring absolute_path = std::filesystem::absolute(to_filesystem_path(target.path), error).make_preferred().wstring();
		const std::wstring parameters = L"/select,\"" + absolute_path + L"\"";
		ShellExecuteW(nullptr, L"open", L"explorer.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL);
	}

	ImGui::EndPopup();
}

void asset_browser::imgui_render_rename_popup()
{
	constexpr const char* popup_name = "Rename##asset_browser";
	if (open_rename_popup)
	{
		ImGui::OpenPopup(popup_name);
		open_rename_popup = false;
	}

	if (!ImGui::BeginPopupModal(popup_name, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	if (ImGui::IsWindowAppearing())
		ImGui::SetKeyboardFocusHere();
	ImGui::SetNextItemWidth(320.0f);
	bool accepted = ImGui::InputText("##name", rename_buffer, IM_ARRAYSIZE(rename_buffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
	if (rename_invalid)
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid name");

	accepted |= ImGui::Button("OK");
	ImGui::SameLine();
	const bool canceled = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);

	if (accepted)
	{
		//	@tool 区切り文字などファイル名に使えない文字は受け付けない
		const std::wstring name = system_utility::convert_utf8_to_utf16(rename_buffer);
		rename_invalid = name.empty() || name == L"." || name == L".." ||
			std::any_of(name.begin(), name.end(), [](wchar_t c) { return c < 0x20 || std::wstring_view(L"\\/:*?\"<>|").find(c) != std::wstring_view::npos; });
		if (!rename_invalid)
		{
			const std::string parent = rename_source_path.substr(0, rename_source_path.rfind('/'));
			pending_move = move_request{ rename_source_path, parent + "/" + system_utility::convert_utf16_to_shift_jis(name) };
			ImGui::CloseCurrentPopup();
		}
	}
	else if (canceled)
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void asset_browser::accept_move_drop(const cache_string& directory)
{
	if (!ImGui::BeginDragDropTarget())
		return;

	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(move_payload_type))
	{
		const std::string source_path(static_cast<const char*>(payload->Data));
		const std::string name = source_path.substr(source_path.rfind('/') + 1);
		pending_move = move_request{ source_path, directory.str() + "/" + name };
	}
	ImGui::EndDragDropTarget();
}

bool asset_browser::move_entry(const std::string& source_path, const std::string& destination_path)
{
	if (resource_manager::make_path_key(source_path) == resource_manager::make_path_key(destination_path))
		return false;

	//	@tool フォルダを自分の下へは移せない
	std::string unused;
	if (resource_manager::replace_base_path(destination_path, source_path, source_path, unused))
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "asset_browser cannot move into itself[%s] -> [%s]", source_path.c_str(), destination_path.c_str());
		return false;
	}

	const std::filesystem::path source = to_filesystem_path(source_path);
	const std::filesystem::path destination = to_filesystem_path(destination_path);
	std::error_code error;
	if (std::filesystem::exists(destination, error))
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "asset_browser destination already exists[%s]", destination_path.c_str());
		return false;
	}

	const bool is_directory = std::filesystem::is_directory(source, error);
	std::filesystem::rename(source, destination, error);
	if (error)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "asset_browser move failed[%s] -> [%s] %s", source_path.c_str(), destination_path.c_str(), error.message().c_str());
		return false;
	}

	if (!is_directory)
	{
		for (auto extension : sidecar_extensions)
		{
			std::filesystem::path sidecar_source = source;
			sidecar_source += std::filesystem::path(std::string(extension));
			if (!std::filesystem::exists(sidecar_source, error))
				continue;

			std::filesystem::path sidecar_destination = destination;
			sidecar_destination += std::filesystem::path(std::string(extension));
			std::filesystem::rename(sidecar_source, sidecar_destination, error);
		}
	}

	//	@tool 対応表を先に更新する(キャッシュのキーは対応表の UUID から作るため)
	if (asset_registry::is_valid())
		asset_registry::instance()->move_path(source_path, destination_path);
	if (resource_manager::is_valid())
		resource_manager::instance()->move_path(source_path, destination_path);

	DEBUG_LOG_CATEGORY(logger::categorys::system, "asset_browser moved[%s] -> [%s]", source_path.c_str(), destination_path.c_str());

	if (resource_manager::make_path_key(selected_path.view()) == resource_manager::make_path_key(source_path))
		selected_path = destination_path;
	pending_refresh = true;
	return true;
}
#endif	//	defined(USE_IMGUI)
