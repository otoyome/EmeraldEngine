#pragma	once

#if defined(USE_IMGUI)
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "System/cache_string.h"

class texture;

//	アセットブラウザ
class asset_browser
{
	SINGLETON(asset_browser)

public:
	//	開き先の登録情報
	struct open_handler
	{
		cache_string name;						//	開き先の名前（開く要求の受け取りに使う）
		cache_string label;						//	メニューの表示名（Shift-JIS）
		std::vector<cache_string> extensions;	//	対象の拡張子（「.」付きの小文字）
		std::function<void()> activate;			//	開き先を有効にする（シーン切替など）
	};

private:
	asset_browser();
	~asset_browser();
	asset_browser(const asset_browser&) = delete;
	asset_browser(asset_browser&&) = delete;

public:
	//	開き先を登録する
	void register_open_handler(open_handler handler);

	//	開く要求を受け取る
	bool take_open_request(std::string_view name, std::string& out_file_path);

	void imgui_render();

private:
	//	フォルダツリー
	struct directory_node
	{
		cache_string name;	//	フォルダ名（Shift-JIS）
		cache_string path;	//	相対パス（Shift-JIS、「/」区切り）
		std::vector<directory_node> children;
	};

	//	現在フォルダ
	struct entry
	{
		cache_string name;			//	ファイル名（Shift-JIS）
		cache_string path;			//	相対パス（Shift-JIS、「/」区切り）
		std::string extension;		//	「.」付きの小文字
		bool is_directory = false;
		std::uintmax_t file_size = 0;

		std::shared_ptr<texture> thumbnail;
		bool thumbnail_requested = false;	//	サムネイルの読み込みを試したか
	};

	//	破棄待ちのサムネイル
	struct retired_thumbnail
	{
		std::shared_ptr<texture> thumbnail;
		std::uint64_t frame = 0;
	};

private:
	void refresh();
	void build_directory_tree(directory_node& node);
	void change_directory(const cache_string& path);
	void scan_entries();
	void retire_thumbnails();
	void release_retired_thumbnails();

	void request_open(const open_handler& handler, const cache_string& file_path);
	const open_handler* find_default_handler(std::string_view extension) const;

	void imgui_render_toolbar();
	void imgui_render_directory_tree(const directory_node& node);
	void imgui_render_entries();
	void imgui_render_entry(entry& target, float cell_width, float cell_height);
	void imgui_render_entry_context_menu(const entry& target);

	void load_thumbnail(entry& target);

private:
	std::vector<open_handler> handlers;
	std::map<std::string, std::string, std::less<>> open_requests;

	directory_node root;
	cache_string current_directory;
	std::vector<entry> entries;
	cache_string selected_path;
	bool reveal_current_directory = true;

	std::vector<retired_thumbnail> retired_thumbnails;
	std::uint64_t frame_index = 0;
	std::chrono::steady_clock::time_point thumbnail_load_deadline{};

	cache_string pending_directory;
	bool pending_refresh = false;

	ImGuiTextFilter filter;
};
#endif	//	defined(USE_IMGUI)
