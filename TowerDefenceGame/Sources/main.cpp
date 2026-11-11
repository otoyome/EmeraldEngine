#include <time.h>

#include "framework.h"
#include "Framework/app_window.h"

#include "System/engine_config_data.h"

namespace
{
	BOOL CALLBACK monitor_enum_proc(HMONITOR hMon, HDC, LPRECT, LPARAM data)
	{
		std::vector<RECT>* monitors = reinterpret_cast<std::vector<RECT>*>(data);

		MONITORINFO mi{};
		mi.cbSize = sizeof(mi);
		if (GetMonitorInfo(hMon, &mi)) {
			monitors->push_back({ mi.rcWork });  // rcMonitor でもOK
		}
		return TRUE;
	}

	bool is_window_fully_inside_any_monitor(HWND hwnd)
	{
		RECT win;
		GetWindowRect(hwnd, &win);

		std::vector<RECT> rcs;
		EnumDisplayMonitors(NULL, NULL, monitor_enum_proc, reinterpret_cast<LPARAM>(&rcs));

		for (auto& rc : rcs)
		{
			// 完全に収まっているか
			if (win.left >= rc.left && win.top >= rc.top &&
				win.right <= rc.right && win.bottom <= rc.bottom)
			{
				return true;
			}
		}
		return false;
	}

	void build_windows_data(HWND hwnd)
	{
		RECT rc;
		GetWindowRect(hwnd, &rc);
		LONG basis_left = engine_config_data::instance()->windows.window_left;
		LONG basis_top = engine_config_data::instance()->windows.window_top;

		SetWindowPos(hwnd,
					 NULL,
					 basis_left,
					 basis_top,
					 rc.right - rc.left,
					 rc.bottom - rc.top,
					 SWP_HIDEWINDOW);
	}
	void make_windows_data(HWND hwnd)
	{
		//RECT& rc = engine_config_data::instance()->windows.window_left;
	}


	void load_engine_config_data(HWND hwnd)
	{
		engine_config_data::create_instance();
		build_windows_data(hwnd);
	}
	void save_engine_config_data(HWND hwnd)
	{
		make_windows_data(hwnd);
		engine_config_data::delete_instance();
	}
}

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_  HINSTANCE prev_instance, _In_ LPSTR cmd_line, _In_ int cmd_show)
{
	srand(static_cast<unsigned int>(time(nullptr)));

#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	//	設定の読込はウィンドウ作成後なので、既定の大きさで作る（保存されている解像度は graphics_process が適用する）
	const engine_config_data::windows_data default_windows;
	app_window window;
	if (!window.create(instance, framework::APPLICATION_NAME, static_cast<int>(default_windows.screen_width), static_cast<int>(default_windows.screen_height)))
		return 0;
	HWND hwnd = window.get_handle();

	//	エンジンの設定ファイル読込
	load_engine_config_data(hwnd);

	//	フレームワーク起動
	framework framework(window);
	bool return_value = framework.run();

	//	エンジンの設定ファイル保存
	save_engine_config_data(hwnd);
	return return_value;
}
