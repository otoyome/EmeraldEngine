#pragma once

#include "input.h"
#include "System/world_singleton.h"

//	@tool ワールドごとに持つ（set_window で結び付けたウィンドウか、それが持つウィンドウが前面にある時だけ入力を受け付ける）
class keyboard
{
	WORLD_SINGLETON(keyboard)

public:
	enum class key_states
	{
		a,
		b,
		c,
		d,
		e,
		f,
		g,
		h,
		i,
		j,
		k,
		l,
		m,
		n,
		o,
		p,
		q,
		r,
		s,
		t,
		u,
		v,
		w,
		x,
		y,
		z,

		shift,
		shiftl,
		shiftr,
		ctrl,
		ctrll,
		ctrlr,
		alt,
		space,

		f1,
		f2,
		f3,
		f4,
		f5,
		f6,
		f7,
		f8,
		f9,
		f10,
		f11,
		f12,
		at,

		max,
	};

	struct states
	{
		input::state key_state[static_cast<size_t>(key_states::max)];

		input::state is_key_state(key_states state) const;
		void set_key_state(key_states state, input::state flag);
	};

public:
	const keyboard::states& get_state() const;
	void set_state(const states& state);

private:
	keyboard();
	~keyboard();

public:
	void update_state();

	//	@tool 入力を受け取るウィンドウを結び付ける
	void set_window(HWND hwnd) { this->hwnd = hwnd; }
	HWND get_window() const { return hwnd; }

private:
	states state;
	HWND hwnd = nullptr;
};

ENUM_ATTR_BITFLAG(keyboard::key_states);
