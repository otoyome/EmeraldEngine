#include "Framework/app_window.h"

#include "System/engine_config_data.h"

app_window::~app_window()
{
	clear_message_hooks();
	if (hwnd && IsWindow(hwnd))
	{
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
		DestroyWindow(hwnd);
	}
	hwnd = nullptr;
	destroy_icons();
}

bool app_window::create(HINSTANCE instance, const wchar_t* class_name, int client_width, int client_height, kinds kind, const wchar_t* title)
{
	this->kind = kind;

	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = &app_window::window_procedure;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = instance;
	wcex.hIcon = 0;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = class_name;
	wcex.hIconSm = 0;
	RegisterClassExW(&wcex);

	const DWORD style = get_windowed_style();
	const int position = kind == kinds::main ? 0 : CW_USEDEFAULT;

	RECT rc{ 0, 0, client_width, client_height };
	//	実際のスタイルで枠の大きさを計算
	AdjustWindowRect(&rc, style, FALSE);
	hwnd = CreateWindowExW(0,
		class_name,
		title ? title : L"",
		style,
		position,	//	CW_USEDEFAULT,
		position,	//	CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		NULL,
		NULL,
		instance,
		NULL);
	if (!hwnd)
		return false;

	SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	return true;
}

void app_window::add_message_hook(message_hook hook)
{
	message_hooks.emplace_back(std::move(hook));
}

void app_window::clear_message_hooks()
{
	message_hooks.clear();
	close_handler = nullptr;
}

void app_window::set_close_handler(std::function<void()> handler)
{
	close_handler = std::move(handler);
}

bool app_window::get_client_size(int& width, int& height) const
{
	RECT rc{};
	if (!hwnd || !GetClientRect(hwnd, &rc))
		return false;
	width = rc.right - rc.left;
	height = rc.bottom - rc.top;
	return true;
}

DWORD app_window::get_windowed_style() const
{
	return kind == kinds::main ? (WS_OVERLAPPEDWINDOW ^ WS_MAXIMIZEBOX ^ WS_THICKFRAME) : WS_OVERLAPPEDWINDOW;
}

void app_window::apply_window_mode(engine_config_data::windows_data::window_modes mode, int client_width, int client_height)
{
	if (!hwnd)
		return;

	const DWORD visible = IsWindowVisible(hwnd) ? WS_VISIBLE : 0;
	if (mode == engine_config_data::windows_data::window_modes::borderless)
	{
		//	ウィンドウがあるモニター全体を覆う
		MONITORINFO monitor_info{};
		monitor_info.cbSize = sizeof(monitor_info);
		GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor_info);
		const RECT& rc = monitor_info.rcMonitor;

		borderless = true;
		SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | visible);
		SetWindowPos(hwnd, HWND_TOP, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);
		return;
	}

	//	枠ありに戻し、保存しておいた位置に描画領域がちょうど指定の大きさになるよう置く
	const DWORD style = get_windowed_style();
	RECT rc{ 0, 0, client_width, client_height };
	AdjustWindowRect(&rc, style, FALSE);

	borderless = false;
	SetWindowLongPtrW(hwnd, GWL_STYLE, style | visible);
	SetWindowPos(hwnd, nullptr,
		engine_config_data::instance()->windows.window_left,
		engine_config_data::instance()->windows.window_top,
		rc.right - rc.left, rc.bottom - rc.top,
		SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool app_window::set_icon(const wchar_t* path)
{
	if (!hwnd || !path || !*path)
		return false;

	//	@tool .ico の中から各サイズに合う画像を選んで読み込む
	HICON loaded_big = static_cast<HICON>(LoadImageW(nullptr, path, IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE));
	HICON loaded_small = static_cast<HICON>(LoadImageW(nullptr, path, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE));
	if (!loaded_big || !loaded_small)
	{
		if (loaded_big)
			DestroyIcon(loaded_big);
		if (loaded_small)
			DestroyIcon(loaded_small);
		return false;
	}

	SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(loaded_big));
	SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(loaded_small));

	//	@tool 前のアイコンは差し替えた後に解放
	destroy_icons();
	icon_big = loaded_big;
	icon_small = loaded_small;
	return true;
}

void app_window::destroy_icons()
{
	if (icon_big)
		DestroyIcon(icon_big);
	if (icon_small)
		DestroyIcon(icon_small);
	icon_big = nullptr;
	icon_small = nullptr;
}

LRESULT CALLBACK app_window::window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	app_window* p{ reinterpret_cast<app_window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)) };
	return p ? p->handle_message(hwnd, msg, wparam, lparam) : DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT app_window::handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	for (const message_hook& hook : message_hooks)
	{
		if (hook(hwnd, msg, wparam, lparam)) { return true; }
	}

	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);

		EndPaint(hwnd, &ps);
		break;
	}
	case WM_CLOSE:
	{
		if (close_handler)
		{
			close_handler();
			break;
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	case WM_DESTROY:
	{
		if (kind == kinds::main)
			PostQuitMessage(0);
		break;
	}
	case WM_CREATE:
	{
		break;
	}
	case WM_KEYDOWN:
	{
		if (kind == kinds::main && wparam == VK_ESCAPE)
		{
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		}
		break;
	}
	case WM_MOUSEWHEEL:
	{
		wheel = GET_WHEEL_DELTA_WPARAM(wparam);
		break;
	}
	case WM_MOVE:
	{
		//	位置を設定に保存するのはアプリケーションウィンドウのみ
		if (kind != kinds::main || borderless)
			return DefWindowProc(hwnd, msg, wparam, lparam);
		engine_config_data::instance()->windows.window_left = (int)(short)LOWORD(lparam);
		engine_config_data::instance()->windows.window_top = (int)(short)HIWORD(lparam);
		break;
	}
	case WM_WINDOWPOSCHANGED:
	{
		if (kind != kinds::main || borderless)
			return DefWindowProc(hwnd, msg, wparam, lparam);
		WINDOWPOS* pos = (WINDOWPOS*)lparam;
		engine_config_data::instance()->windows.window_left = pos->x;
		engine_config_data::instance()->windows.window_top = pos->y;
		break;
	}

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}
