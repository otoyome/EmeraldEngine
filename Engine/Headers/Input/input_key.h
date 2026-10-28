#pragma once

#include "keyboard.h"
#include "mouse.h"
#include "gamepad.h"

namespace input
{
	//	キーの出どころ
	enum class sources : uint8_t
	{
		keyboard,
		mouse,
		mouse_axis,
		gamepad,
		gamepad_axis,
	};

	//	機器を問わずキーを指す
	struct key
	{
		sources source;
		uint8_t code;

		constexpr key(keyboard::key_states value) : source(sources::keyboard), code(static_cast<uint8_t>(value)) {}
		constexpr key(mouse::key_states value) : source(sources::mouse), code(static_cast<uint8_t>(value)) {}
		constexpr key(mouse::axis_states value) : source(sources::mouse_axis), code(static_cast<uint8_t>(value)) {}
		constexpr key(gamepad::key_states value) : source(sources::gamepad), code(static_cast<uint8_t>(value)) {}
		constexpr key(gamepad::axis_states value) : source(sources::gamepad_axis), code(static_cast<uint8_t>(value)) {}

		constexpr bool operator==(const key& other) const { return source == other.source && code == other.code; }
		constexpr bool operator!=(const key& other) const { return !(*this == other); }
	};

	//	キーの状態（軸は量の絶対値が押下とみなす量を超えたら押下）
	state get_key_state(key k, size_t pad_index = 0);

	//	キーの量（ボタンは 0 か 1、軸はその値）
	float get_key_value(key k, size_t pad_index = 0);
}
