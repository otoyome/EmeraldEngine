#include "input/mouse.h"

#if defined(USE_IMGUI)
#include "Input/keyboard.h"
#endif	//	defined(USE_IMGUI)

namespace
{
	static const int KeyMap[] =
	{
		VK_LBUTTON,
		VK_MBUTTON,
		VK_RBUTTON,
#if _WIN32_WINNT >= 0x0500
		VK_XBUTTON1,
		VK_XBUTTON2,
#endif  //  _WIN32_WINNT >= 0x0500
	};
}

input::state mouse::states::is_key_state(key_states state) const
{
	return key_state[static_cast<size_t>(state)];
}

void mouse::states::set_key_state(key_states state, input::state flag)
{
	key_state[static_cast<size_t>(state)] = flag;
}

mouse::states mouse::get_state() const
{
	states state;
	state.position = position_state;
	memcpy_s(state.key_state, sizeof(state.key_state), key_state, sizeof(key_state));
	return state;
}

void mouse::set_state(const states& state)
{
	this->position_state = state.position;
	memcpy_s(key_state, sizeof(key_state), state.key_state, sizeof(state.key_state));
}

math::int2 mouse::get_move_cursol() const
{
	return {move_vector.x, move_vector.y};
}

int mouse::get_wheel_value() const
{
	return position_state.wheel;
}

float mouse::get_axis(axis_states state) const
{
	switch (state)
	{
		case axis_states::x:
			return static_cast<float>(move_vector.x);
		case axis_states::y:
			return static_cast<float>(move_vector.y);
		case axis_states::wheel:
			return static_cast<float>(position_state.wheel);
	}
	return 0.0f;
}

input::state mouse::get_axis_state(axis_states state) const
{
	return axis_key_state[static_cast<size_t>(state)];
}

void mouse::set_center_lock(bool lock)
{
	this->window_center_lock = lock;
}

bool mouse::get_center_lock() const
{
	return window_center_lock;
}

void mouse::set_cursol_hide(bool hide)
{
	cursol_hide = hide;
}

bool mouse::get_cursol_hide() const
{
	return cursol_hide;
}

mouse::mouse(HWND hwnd)
: hwnd(hwnd)
, window_center_lock(false)
, cursol_hide(false)
, wheel_value(0)
, client_size()
, position_state()
{
	RECT rc;
	GetClientRect(hwnd, &rc);
	client_size.x = rc.right - rc.left;
	client_size.y = rc.bottom - rc.top;

	//	ホイールは Raw Input で受け取る
	//	INPUTSINK は前面でなくても届くので、受け取る時に自プロセスの窓が前面かを調べる
	RAWINPUTDEVICE device = {};
	device.usUsagePage = 0x01;	//	Generic Desktop
	device.usUsage = 0x02;		//	マウス
	device.dwFlags = RIDEV_INPUTSINK;
	device.hwndTarget = hwnd;
	::RegisterRawInputDevices(&device, 1, sizeof(device));
}

mouse::~mouse()
{
	RAWINPUTDEVICE device = {};
	device.usUsagePage = 0x01;
	device.usUsage = 0x02;
	device.dwFlags = RIDEV_REMOVE;
	device.hwndTarget = nullptr;
	::RegisterRawInputDevices(&device, 1, sizeof(device));
}

math::int2 mouse::get_client_size() const
{
	return client_size;
}

void mouse::update_state()
{
#if defined(USE_IMGUI)
	bool window_center_lock = this->window_center_lock;
	bool cursol_hide = this->cursol_hide;

	//	F1入力時に解除
	auto keyboard_state = keyboard::instance()->get_state();
	static bool force_unlock = false;
	if (keyboard_state.is_key_state(keyboard::key_states::f1) == input::state::down)
	{
		force_unlock = !force_unlock;
	}
	if (force_unlock)
	{
		window_center_lock = false;
		cursol_hide = false;
	}
#endif	//	defined(USE_IMGUI)

	bool current_cursol_hide = cursol_hide;
	position_states new_position_state;
	{
		RECT rc;
		GetClientRect(hwnd, &rc);
		UINT screen_width = rc.right - rc.left;
		UINT screen_height = rc.bottom - rc.top;
		client_size = { static_cast<int>(screen_width), static_cast<int>(screen_height) };
		bool input_key_state[magic_enum::enum_count<key_states>()];
		//	自プロセスの窓が前面なら受け付ける
		if (input::is_process_foreground())
		{
			//	入力状況
			for (size_t index = 0; index < magic_enum::enum_count<key_states>(); ++index)
			{
				input_key_state[index] = ::GetAsyncKeyState(KeyMap[index]) & 0x8000;
			}

			//	カーソル座標
			POINT cursor;
			::GetCursorPos(&cursor);

			//	移動ベクトルを算出
			if (window_center_lock)
			{
				//	クライアント中心をスクリーン座標に変換
				POINT center{ static_cast<LONG>(screen_width / 2), static_cast<LONG>(screen_height / 2) };
				::ClientToScreen(hwnd, &center);

				//	マウス座標を中心点に固定化
				move_vector.x = cursor.x - center.x;
				move_vector.y = cursor.y - center.y;

				::SetCursorPos(center.x, center.y);
			}

			//	座標算出
			::ScreenToClient(hwnd, &cursor);
			new_position_state.position.x = cursor.x;
			new_position_state.position.y = cursor.y;
			if (!window_center_lock)
			{
				move_vector.x = new_position_state.position.x - position_state.position.x;
				move_vector.y = new_position_state.position.y - position_state.position.y;
			}

			//	ホイール
			{
				new_position_state.wheel = wheel_value;
				wheel_value = 0;
			}
		}
		else
		{
			//	テキトーに初期化しておく
			move_vector.x = 0;
			move_vector.y = 0;
			POINT cursor;
			::GetCursorPos(&cursor);
			memset(input_key_state, 0, sizeof(input_key_state));

			//	座標算出
			::ScreenToClient(hwnd, &cursor);
			new_position_state.position.x = cursor.x;
			new_position_state.position.y = cursor.y;

			//	カーソル表示化
			current_cursol_hide = false;
		}

		//	入力状況を保存
		for (size_t index = 0; index < magic_enum::enum_count<key_states>(); ++index)
		{
			key_state[index] = input::next_state(key_state[index], input_key_state[index]);
		}
	}

	//	カーソル表示/非表示処理
	static bool cursol_hide_inner = !cursol_hide;
	if (cursol_hide_inner != current_cursol_hide)
	{
		cursol_hide_inner = current_cursol_hide;

		int count;
		do
		{
			count = ::ShowCursor(!current_cursol_hide);
		}
		while ((current_cursol_hide && count >= 0) || (!current_cursol_hide && count < 0));
	}

	position_state = new_position_state;

	//	軸を押下とみなした状態
	for (size_t index = 0; index < magic_enum::enum_count<axis_states>(); ++index)
	{
		const bool pressed = std::abs(get_axis(static_cast<axis_states>(index))) >= input::pressed_threshold;
		axis_key_state[index] = input::next_state(axis_key_state[index], pressed);
	}
}

LRESULT mouse::window_proccess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_INPUT:
		{
			//	ホイールの量を足す
			RAWINPUT raw = {};
			UINT size = sizeof(raw);
			if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1)
				&& raw.header.dwType == RIM_TYPEMOUSE && (raw.data.mouse.usButtonFlags & RI_MOUSE_WHEEL) && input::is_process_foreground())
				wheel_value += static_cast<SHORT>(raw.data.mouse.usButtonData);
			return 0;
		}
#if 0
		case WM_MOUSEHWHEEL:
		{
			io.AddMouseWheelEvent(-(float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, 0.0f);
			return 0;
		}
#endif
	}
	return 0;
}
