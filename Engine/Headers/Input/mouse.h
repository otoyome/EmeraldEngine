#pragma once

#include "input.h"

class mouse
{
	SINGLETON_DECREATE(mouse)

public:
	enum class key_states
	{
		left,
		middle,
		right,
		x_button_1,
		x_button_2,
	};

	//	量を持つ入力ステート
	enum class axis_states
	{
		x,
		y,
		wheel,
	};
	struct position_states
	{
		math::int2 position = { 0, 0 }; // センターロック時は中心点からの移動量が保存されている
		int wheel = 0;
	};

	struct states
	{
		position_states position;
		input::state key_state[magic_enum::enum_count<key_states>()] = {};

		input::state is_key_state(key_states state) const;
		void set_key_state(key_states state, input::state flag);
	};

public:
	states get_state() const;
	void set_state(const states& state);

	math::int2 get_move_cursol() const;
	int get_wheel_value() const;

	float get_axis(axis_states state) const;
	input::state get_axis_state(axis_states state) const;

	void set_center_lock(bool lock);
	bool get_center_lock() const;

	void set_cursol_hide(bool hide);
	bool get_cursol_hide() const;

	math::int2 get_client_size() const;

public:
	static	inline	void	create_instance(HWND hwnd)
	{
		if (!instance_ptr)
		{
			instance_ptr = new mouse(hwnd);
		}
	}

private:
	mouse(HWND hwnd);
	~mouse();

public:
	void update_state();

	LRESULT window_proccess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	HWND hwnd;
	bool window_center_lock;
	bool cursol_hide;
	int wheel_value;
	math::int2 client_size;
	math::int2 move_vector;
	position_states position_state;
	input::state key_state[magic_enum::enum_count<key_states>()];
	input::state axis_key_state[magic_enum::enum_count<axis_states>()] = {};
};

ENUM_ATTR_BITFLAG(mouse::key_states);
