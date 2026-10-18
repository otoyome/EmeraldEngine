#include "Framework/app_window.h"

#include "System/engine_config_data.h"

app_window::~app_window()
{
	//	@tool 破棄の途中のメッセージで、登録した処理（破棄済みの物を参照する事がある）を呼ばない
	clear_message_hooks();
	if (hwnd && IsWindow(hwnd))
	{
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
		DestroyWindow(hwnd);
	}
	hwnd = nullptr;
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

	//	@tool 道具用のウィンドウは大きさを変えられ、位置はシステムに任せる
	const DWORD style = kind == kinds::main ? (WS_OVERLAPPEDWINDOW ^ WS_MAXIMIZEBOX ^ WS_THICKFRAME) : WS_OVERLAPPEDWINDOW;
	const int position = kind == kinds::main ? 0 : CW_USEDEFAULT;

	RECT rc{ 0, 0, client_width, client_height };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
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
		//	@tool 閉じる操作を任されている場合は、ウィンドウを壊さずに知らせる
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
		//	@tool 位置を設定に保存するのはアプリケーションのウィンドウだけ
		if (kind != kinds::main)
			return DefWindowProc(hwnd, msg, wparam, lparam);
		engine_config_data::instance()->windows.window_left = (int)(short)LOWORD(lparam);
		engine_config_data::instance()->windows.window_top = (int)(short)HIWORD(lparam);
		break;
	}
	case WM_WINDOWPOSCHANGED:
	{
		if (kind != kinds::main)
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
