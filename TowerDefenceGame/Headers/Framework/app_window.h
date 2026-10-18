#pragma once

#include <functional>
#include <vector>

//	@tool アプリケーションのウィンドウ（ウィンドウの生成とメッセージの処理）
//	@tool メッセージは登録した処理へ順に渡し、どれも処理しなかった物をここで処理する
class app_window
{
public:
	//	@tool メッセージを先に受け取る処理（true を返すとそこで処理済みにする）
	using message_hook = std::function<bool(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)>;

	//	@tool ウィンドウの種類
	enum class kinds
	{
		main,	//	@tool アプリケーションのウィンドウ（破棄でアプリケーションを終える。大きさは固定。位置を設定に保存する。ESC で閉じる）
		tool,	//	@tool 道具用のウィンドウ（大きさを変えられる。閉じる操作は set_close_handler の処理に任せる）
	};

public:
	app_window() = default;
	~app_window();

	app_window(const app_window&) = delete;
	app_window& operator=(const app_window&) = delete;
	app_window(app_window&&) noexcept = delete;
	app_window& operator=(app_window&&) noexcept = delete;

	//	@tool ウィンドウクラスを登録してウィンドウを作る（描画領域の大きさを指定する。表示はしない）
	bool create(HINSTANCE instance, const wchar_t* class_name, int client_width, int client_height, kinds kind = kinds::main, const wchar_t* title = nullptr);

	HWND get_handle() const { return hwnd; }
	//	@tool 直前に受け取ったマウスホイールの回転量
	float get_wheel() const { return wheel; }

	//	@tool メッセージを先に受け取る処理を足す（足した順に呼ぶ）
	void add_message_hook(message_hook hook);
	void clear_message_hooks();
	//	@tool 閉じる操作（WM_CLOSE）を受け取る処理を設定する（設定するとウィンドウは壊さない）
	void set_close_handler(std::function<void()> handler);

	//	@tool 描画領域の大きさ
	bool get_client_size(int& width, int& height) const;

private:
	static LRESULT CALLBACK window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	HWND hwnd = nullptr;
	kinds kind = kinds::main;
	std::vector<message_hook> message_hooks;
	std::function<void()> close_handler;
	float wheel{ 0 };
};
