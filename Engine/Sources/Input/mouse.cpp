#include "input/mouse.h"

#if defined(USE_IMGUI)
#include "keyboard.h"
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

DirectX::XMINT2 mouse::get_move_cursol() const
{
	return {move_vector.x, move_vector.y};
}

int mouse::get_wheel_value() const
{
	return position_state.wheel;
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
, viewport_size()
, position_state()
{
	RECT rc;
	GetClientRect(hwnd, &rc);
	viewport_size.x = rc.right - rc.left;
	viewport_size.y = rc.bottom - rc.top;
}

mouse::~mouse()
{
}

void mouse::set_viewport_size(const DirectX::XMINT2& size)
{
	viewport_size = size;
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
		bool input_key_state[static_cast<int>(key_states::max)];
		if (::GetForegroundWindow() == hwnd)
		{
			//	入力状況
			for (size_t index = 0; index < static_cast<size_t>(key_states::max); ++index)
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
			new_position_state.position.x = rc.left + static_cast<LONG>(cursor.x / static_cast<float>(viewport_size.x) * static_cast<float>(screen_width));
			new_position_state.position.y = rc.top + static_cast<LONG>(cursor.y / static_cast<float>(viewport_size.y) * static_cast<float>(screen_height));
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
			new_position_state.position.x = static_cast<LONG>(cursor.x / static_cast<float>(viewport_size.x) * static_cast<float>(screen_width));
			new_position_state.position.y = static_cast<LONG>(cursor.y / static_cast<float>(viewport_size.y) * static_cast<float>(screen_height));

			//	カーソル表示化
			current_cursol_hide = false;
		}

		//	入力状況を保存
		for (size_t index = 0; index < static_cast<size_t>(key_states::max); ++index)
		{
			if (input_key_state[index])
			{
				if (key_state[index] == input::state::none || key_state[index] == input::state::release)
				{
					key_state[index] = input::state::down;
				}
				else if (key_state[index] == input::state::down)
				{
					key_state[index] = input::state::hold;
				}
			}
			else
			{
				if (key_state[index] == input::state::down || key_state[index] == input::state::hold)
				{
					key_state[index] = input::state::release;
				}
				else
				{
					key_state[index] = input::state::none;
				}
			}
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
}

LRESULT mouse::window_proccess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_MOUSEWHEEL:
		{
			wheel_value += GET_WHEEL_DELTA_WPARAM(wParam);
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
