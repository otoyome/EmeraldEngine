#pragma once

#include "input.h"
#include "System/world_singleton.h"

//	@tool ワールドごとに持つ（set_window で結び付けたウィンドウが前面にある時だけ入力を受け付ける）
class mouse
{
	WORLD_SINGLETON(mouse)

public:
	enum class key_states
	{
		left,
		middle,
		right,
		x_button_1,
		x_button_2,

		max
	};
	struct position_states
	{
		math::int2 position = { 0, 0 }; // センターロック時は中心点からの移動量が保存されている
		int wheel = 0;
	};

	struct states
	{
		position_states position;
		input::state key_state[static_cast<int>(key_states::max)] = {};

		input::state is_key_state(key_states state) const;
		void set_key_state(key_states state, input::state flag);
	};

public:
	states get_state() const;
	void set_state(const states& state);

	math::int2 get_move_cursol() const;
	int get_wheel_value() const;

	void set_center_lock(bool lock);
	bool get_center_lock() const;

	void set_cursol_hide(bool hide);
	bool get_cursol_hide() const;

private:
	mouse();
	~mouse();

public:
	//	@tool 入力を受け取るウィンドウを結び付ける（描画領域の大きさも取り直す）
	void set_window(HWND hwnd);
	HWND get_window() const { return hwnd; }

	void set_viewport_size(const math::int2& size);

	void update_state();

	LRESULT window_proccess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	HWND hwnd = nullptr;
	bool window_center_lock;
	bool cursol_hide;
	//	@tool 最後に反映したカーソルの表示状態（ワールドごとに持ち、他のワールドの状態と打ち消し合わないようにする）
	bool cursol_hide_inner;
	int wheel_value;
	math::int2 viewport_size;
	math::int2 move_vector;
	position_states position_state;
	input::state key_state[static_cast<int>(key_states::max)];
};

ENUM_ATTR_BITFLAG(mouse::key_states);
