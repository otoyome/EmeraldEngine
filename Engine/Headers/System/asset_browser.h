#pragma	once

#if defined(USE_IMGUI)
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "System/cache_string.h"
#include "System/universally_unique_identifier.h"
#include "System/asset_thumbnail_cache.h"

class texture;

//	アセットブラウザ
class asset_browser
{
	SINGLETON(asset_browser)

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
		cache_string name;			//	ファイル名
		cache_string path;			//	相対パス
		cache_string extension;		//	拡張子
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

	//	描画の後に行う移動・名前変更
	struct move_request
	{
		cache_string source_path;		//	移動元
		cache_string destination_path;	//	移動先
	};

	//	現在のフォルダまでの経路の一段
	struct breadcrumb
	{
		cache_string name;	//	フォルダ名
		cache_string path;	//	相対パス
	};

	//	ファイルオープンリクエスト
	struct open_request
	{
		universally_unique_identifier uuid;
		std::string file_path;
	};
	//	新規作成
	struct create_request
	{
		std::size_t handler_index = 0;	//	作成処理の番号（folder_handler_index ならフォルダ）
		cache_string file_path;			//	作るファイル
	};
public:
	//	ドラッグ&ドロップの種類名(中身は「/」区切りの相対パス、Shift-JIS)
	static constexpr const char* drag_payload_type = "ASSET_BROWSER_PATH";

	//	開き先の登録情報
	struct open_handler
	{
		cache_string name;						//	開き先の名前
		cache_string label;						//	メニューの表示名
		std::vector<cache_string> extensions;	//	対象の拡張子
		std::function<void()> activate;			//	開く(@remainder 現状はシーンを変えているが、その内別ウィンドウを開くように対応)
	};

	//	新規作成の登録情報
	struct create_handler
	{
		cache_string label;										//	メニューの表示名
		cache_string default_name;								//	名前の初期値
		cache_string extension;									//	作るファイルの拡張子
		std::function<bool(std::string_view file_path)> create;	//	ファイル作成
	};

private:
	//	作成処理の番号でフォルダを表す値
	static constexpr std::size_t folder_handler_index = static_cast<std::size_t>(-1);

	asset_browser();
	~asset_browser();
	asset_browser(const asset_browser&) = delete;
	asset_browser(asset_browser&&) = delete;

public:
	//	開き先を登録する
	void register_open_handler(open_handler handler);

	//	新規作成を登録する
	void register_create_handler(create_handler handler);

	//	開く要求を受け取る
	bool take_open_request(std::string_view name, std::string& out_file_path);

	//	UUID のアセットを既定の開き先で開く
	bool open_asset(const universally_unique_identifier& uuid);

	void imgui_render();

private:
	void refresh();
	void build_directory_tree(directory_node& node);
	void change_directory(const cache_string& path);
	void build_breadcrumbs();
	void scan_entries();
	void retire_thumbnails();
	void release_retired_thumbnails();

	//	ファイルオープンリクエスト
	void request_open(const open_handler& handler, const cache_string& file_path);
	const open_handler* find_default_handler(std::string_view extension) const;

	//	ドラッグされたエントリーを落としたフォルダへ移動
	void accept_move_drop(const cache_string& directory);

	//	ファイル/フォルダを移動し、対応表と読込済みリソースのパスも合わせる
	bool move_entry(const cache_string& source_path, const cache_string& destination_path);

	//	登録した処理でファイルを作る
	bool create_entry(const create_request& request);

	//	アセットファイルを別名で複製し、UUID を振り直す
	bool duplicate_entry(const cache_string& source_path);

	void load_thumbnail(entry& target);

private:	//	IMGUI系
	void imgui_render_toolbar();
	void imgui_render_directory_tree(const directory_node& node);
	void imgui_render_entries();
	void imgui_render_entry(entry& target, float cell_width, float cell_height);
	void imgui_render_entry_context_menu(const entry& target);
	void imgui_render_rename_popup();
	void imgui_render_create_menu(const cache_string& directory);
	void imgui_render_create_popup();

private:
	std::vector<open_handler> handlers;
	std::vector<create_handler> create_handlers;
	std::map<std::string, open_request, std::less<>> open_requests;

	directory_node root;
	cache_string current_directory;
	std::vector<breadcrumb> breadcrumbs;
	std::vector<entry> entries;
	cache_string selected_path;
	bool reveal_current_directory = true;

	std::vector<retired_thumbnail> retired_thumbnails;
	std::uint64_t frame_index = 0;
	std::chrono::steady_clock::time_point thumbnail_load_deadline{};
	asset_thumbnail_cache thumbnail_cache;

	cache_string pending_directory;
	bool pending_refresh = false;
	std::optional<move_request> pending_move;

	cache_string rename_source_path;
	cache_string rename_name;
	bool open_rename_popup = false;
	bool rename_invalid = false;

	std::optional<create_request> pending_create;
	std::optional<cache_string> pending_duplicate;
	cache_string create_directory;
	cache_string create_name;
	std::size_t create_handler_index = 0;
	bool open_create_popup = false;
	bool create_invalid = false;

	ImGuiTextFilter filter;
};
#endif	//	defined(USE_IMGUI)
