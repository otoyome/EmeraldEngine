#pragma once

#include <functional>
#include <vector>

#include "System/engine_config_data.h"

//	アプリケーションのウィンドウ
class app_window
{
public:
	//	メッセージを先に受け取る処理
	using message_hook = std::function<bool(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)>;

	//	ウィンドウの種類
	enum class kinds
	{
		main,	//	アプリケーションのウィンドウ
		tool,	//	道具用のウィンドウ
	};
	
public:
	HWND get_handle() const { return hwnd; }
	float get_wheel() const { return wheel; }

public:
	app_window() = default;
	~app_window();

	app_window(const app_window&) = delete;
	app_window& operator=(const app_window&) = delete;
	app_window(app_window&&) noexcept = delete;
	app_window& operator=(app_window&&) noexcept = delete;

	//	ウィンドウクラスを登録してウィンドウ作成
	bool create(HINSTANCE instance, const wchar_t* class_name, int client_width, int client_height, kinds kind = kinds::main, const wchar_t* title = nullptr);

	//	メッセージを先に受け取る処理を足す
	void add_message_hook(message_hook hook);
	void clear_message_hooks();

	//	閉じる操作（WM_CLOSE）を受け取る処理を設定
	void set_close_handler(std::function<void()> handler);

	//	描画領域の大きさ
	bool get_client_size(int& width, int& height) const;

	//	表示の方法を切り替える
	void apply_window_mode(engine_config_data::windows_data::window_modes mode, int client_width, int client_height);

	//	アイコンを .ico ファイルから設定
	bool set_icon(const wchar_t* path);

private:
	//	ウィンドウスタイル取得
	DWORD get_windowed_style() const;

	//	読み込んだアイコンを解放
	void destroy_icons();

	static LRESULT CALLBACK window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	HWND hwnd = nullptr;
	kinds kind = kinds::main;
	bool borderless = false;
	std::vector<message_hook> message_hooks;
	std::function<void()> close_handler;
	float wheel{ 0 };
	HICON icon_big = nullptr;	//	タイトルバー・タスクバー用のアイコン
	HICON icon_small = nullptr;	//	小さいアイコン
};
