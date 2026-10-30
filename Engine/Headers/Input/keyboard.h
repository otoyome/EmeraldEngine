#pragma once

#include "input.h"

class keyboard
{
	SINGLETON(keyboard)

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

		//	f1～f12 は連続させる
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
		at,			//	JIS 配列は @、US 配列は `

		//	上段の数字
		digit_0,
		digit_1,
		digit_2,
		digit_3,
		digit_4,
		digit_5,
		digit_6,
		digit_7,
		digit_8,
		digit_9,

		//	修飾キーの残り
		altl,
		altr,
		winl,
		winr,
		apps,

		//	基本操作
		enter,
		escape,
		tab,
		backspace,
		caps_lock,

		//	矢印
		up,
		down,
		left,
		right,

		//	編集・移動
		insert,
		del,
		home,
		end,
		page_up,
		page_down,

		//	テンキー
		numpad_0,
		numpad_1,
		numpad_2,
		numpad_3,
		numpad_4,
		numpad_5,
		numpad_6,
		numpad_7,
		numpad_8,
		numpad_9,
		numpad_add,
		numpad_subtract,
		numpad_multiply,
		numpad_divide,
		numpad_decimal,
		num_lock,

		//	記号。配列で刻印が変わるので仮想キーコードの名前にする（JIS / US）
		oem_minus,	//	- / -
		oem_plus,	//	; / =
		oem_comma,	//	, / ,
		oem_period,	//	. / .
		oem_1,		//	: / ;
		oem_2,		//	/ / /
		oem_4,		//	[ / [
		oem_5,		//	￥ / \（バックスラッシュ）
		oem_6,		//	] / ]
		oem_7,		//	^ / '
		oem_102,	//	＼（ろ） / なし

		//	日本語キーボード
		convert,
		non_convert,

		//	その他
		scroll_lock,
		pause,
	};

	struct states
	{
		input::state key_state[magic_enum::enum_count<key_states>()] = {};

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

private:
	states state;
};

ENUM_ATTR_BITFLAG(keyboard::key_states);
