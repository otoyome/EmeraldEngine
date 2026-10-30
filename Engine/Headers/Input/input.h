#pragma once

namespace input
{
	enum class state
	{
		none,
		down,
		hold,
		release,
	};

	//	量を持つ入力を押下とみなす量
	constexpr float pressed_threshold = 0.5f;

	//	現在の状態から次の状態を求める
	inline state next_state(state current, bool pressed)
	{
		if (pressed)
			return current == state::none || current == state::release ? state::down : state::hold;
		return current == state::down || current == state::hold ? state::release : state::none;
	}

	//	自プロセスの窓が前面か
	inline bool is_process_foreground()
	{
		HWND foreground = ::GetForegroundWindow();
		if (!foreground)
			return false;
		DWORD process_id = 0;
		::GetWindowThreadProcessId(foreground, &process_id);
		return process_id == ::GetCurrentProcessId();
	}
}
