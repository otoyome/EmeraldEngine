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

#include "Core/cache_string.h"
#include "System/asset_thumbnail_cache.h"

class texture;

//	アセットブラウザ
class asset_browser
{
	SINGLETON(asset_browser)

	//	フォルダツリー
	struct directory_node
	{
		core::cache_string name;	//	フォルダ名（Shift-JIS）
		core::cache_string path;	//	相対パス（Shift-JIS、「/」区切り）
		std::vector<directory_node> children;
	};

	//	現在フォルダ
	struct entry
	{
		core::cache_string name;			//	ファイル名
		core::cache_string path;			//	相対パス
		core::cache_string extension;		//	拡張子
		bool is_directory = false;
		std::uintmax_t file_size = 0;

		std::shared_ptr<texture> thumbnail;
		bool thumbnail_requested = false;	//	サムネイルの読み込みを試したか
	};

	//	描画の後に行う移動・名前変更
	struct move_request
	{
		core::cache_string source_path;		//	移動元
		core::cache_string destination_path;	//	移動先
	};

	//	現在のフォルダまでの経路の一段
	struct breadcrumb
	{
		core::cache_string name;	//	フォルダ名
		core::cache_string path;	//	相対パス
	};

	//	新規作成
	struct create_request
	{
		std::size_t handler_index = 0;	//	作成処理の番号（folder_handler_index ならフォルダ）
		core::cache_string file_path;			//	作るファイル
	};
public:
	//	ドラッグ&ドロップの種類名(中身は「/」区切りの相対パス、Shift-JIS)
	static constexpr const char* drag_payload_type = "ASSET_BROWSER_PATH";

	//	開き先の登録情報
	struct open_handler
	{
		core::cache_string label;						//	メニューの表示名
		std::vector<core::cache_string> extensions;	//	対象の拡張子
		std::function<void(const core::cache_string& file_path)> open_file;	//	ファイルを直接開く
	};

	//	削除してよいかを調べる関数
	using delete_validator = std::function<bool(std::string_view file_path, std::string& out_reason)>;

	//	ファイル/フォルダの移動・削除を知らせる先
	struct path_listener
	{
		std::function<void(std::string_view old_path, std::string_view new_path)> moved;
		std::function<void(std::string_view path)> deleted;
	};

	//	新規作成の登録情報
	struct create_handler
	{
		core::cache_string label;										//	メニューの表示名
		core::cache_string default_name;								//	名前の初期値
		core::cache_string extension;									//	作るファイルの拡張子
		std::function<bool(std::string_view file_path)> create;	//	ファイル作成
	};

	//	エンジンアセット変換の登録情報
	struct convert_handler
	{
		core::cache_string label;						//	メニューの表示名
		std::vector<core::cache_string> extensions;	//	対象の拡張子
		std::function<core::cache_string(const core::cache_string& file_path)> output_path;		//	変換先のパス
		std::function<void(const core::cache_string& file_path)> imgui_render_options;	//	ポップアップ内の設定表示
		std::function<bool(const core::cache_string& file_path)> convert;					//	変換
	};

private:
	//	作成処理の番号でフォルダを表す値
	static constexpr std::size_t folder_handler_index = static_cast<std::size_t>(-1);

	asset_browser();
	~asset_browser();
	asset_browser(const asset_browser&) = delete;
	asset_browser(asset_browser&&) = delete;

public:
	//	開き先を登録
	void register_open_handler(open_handler handler);

	//	新規作成を登録
	void register_create_handler(create_handler handler);

	//	変換を登録
	void register_convert_handler(convert_handler handler);

	//	削除の可否を調べる処理を登録
	void register_delete_validator(delete_validator validator);

	//	移動・削除の通知先を登録
	void register_path_listener(path_listener listener);


	//	アセットエディターを開く
	bool open_asset(const core::uuid& uuid);

	void imgui_render(bool& open);

private:
	void refresh();
	void build_directory_tree(directory_node& node);
	void change_directory(const core::cache_string& path);
	void apply_directory(const core::cache_string& path);
	void go_back();
	void go_forward();
	void build_breadcrumbs();
	void scan_entries();

	//	ファイルオープンリクエスト
	void request_open(const open_handler& handler, const core::cache_string& file_path);
	const open_handler* find_default_handler(std::string_view extension) const;

	//	フォルダ移動
	void accept_move_drop(const core::cache_string& directory);

	//	ファイル/フォルダを移動
	bool move_entry(const core::cache_string& source_path, const core::cache_string& destination_path);

	//	ファイル作成
	bool create_entry(const create_request& request);

	//	アセットファイルを別名で複製
	bool duplicate_entry(const core::cache_string& source_path);

	//	削除するファイルを集める
	std::vector<core::cache_string> collect_delete_files(const core::cache_string& path) const;

	//	アセット破棄処理
	bool delete_entry(const core::cache_string& path);

	void load_thumbnail(entry& target);

private:	//	IMGUI系
	void imgui_render_toolbar();
	void imgui_render_directory_tree(const directory_node& node);
	void imgui_render_entries();
	void imgui_render_entry(entry& target, float cell_width, float cell_height);
	void imgui_render_entry_context_menu(const entry& target);
	void imgui_render_rename_popup();
	void imgui_render_create_menu(const core::cache_string& directory);
	void imgui_render_create_popup();
	void imgui_render_delete_popup();
	void imgui_render_convert_popup();

private:
	std::vector<open_handler> handlers;
	std::vector<create_handler> create_handlers;
	std::vector<convert_handler> convert_handlers;
	std::vector<delete_validator> delete_validators;
	std::vector<path_listener> path_listeners;

	directory_node root;
	core::cache_string current_directory;
	std::vector<core::cache_string> back_history;
	std::vector<core::cache_string> forward_history;
	std::vector<breadcrumb> breadcrumbs;
	std::vector<entry> entries;
	core::cache_string selected_path;
	bool reveal_current_directory = true;

public:	//	サムネイル
	std::chrono::steady_clock::time_point thumbnail_load_deadline{};
	asset_thumbnail_cache thumbnail_cache;

public:	//	移動・更新・履歴操作の要求
	core::cache_string pending_directory;
	bool pending_refresh = false;
	bool pending_back = false;
	bool pending_forward = false;
	std::optional<move_request> pending_move;

public:	//	名前変更ポップアップ
	core::cache_string rename_source_path;
	core::cache_string rename_name;
	bool open_rename_popup = false;
	bool rename_invalid = false;

public:	//	作成・複製ポップアップ
	std::optional<create_request> pending_create;
	std::optional<core::cache_string> pending_duplicate;
	core::cache_string create_directory;
	core::cache_string create_name;
	std::size_t create_handler_index = 0;
	bool open_create_popup = false;
	bool create_invalid = false;

public:	//	削除ポップアップ
	std::optional<core::cache_string> pending_delete;
	core::cache_string delete_path;
	std::vector<std::string> delete_reasons;
	bool open_delete_popup = false;

public:	//	変換ポップアップ
	bool pending_convert = false;
	core::cache_string convert_path;
	std::size_t convert_handler_index = 0;
	bool open_convert_popup = false;

	ImGuiTextFilter filter;
};
#endif	//	defined(USE_IMGUI)
