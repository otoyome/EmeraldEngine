#pragma once

#include "keyboard.h"
#include "mouse.h"
#include "gamepad.h"

namespace input
{
	//	キーデバイスタイプ
	enum class devices : uint8_t
	{
		keyboard,
		mouse,
		mouse_axis,
		gamepad,
		gamepad_axis,
	};

	//	キー情報
	struct key
	{
		devices device;	//	デバイス指定
		uint8_t code;	//	キーコード

		constexpr key(keyboard::key_states value) : device(devices::keyboard), code(static_cast<uint8_t>(value)) {}
		constexpr key(mouse::key_states value) : device(devices::mouse), code(static_cast<uint8_t>(value)) {}
		constexpr key(mouse::axis_states value) : device(devices::mouse_axis), code(static_cast<uint8_t>(value)) {}
		constexpr key(gamepad::key_states value) : device(devices::gamepad), code(static_cast<uint8_t>(value)) {}
		constexpr key(gamepad::axis_states value) : device(devices::gamepad_axis), code(static_cast<uint8_t>(value)) {}

		constexpr bool operator==(const key& other) const { return device == other.device && code == other.code; }
		constexpr bool operator!=(const key& other) const { return !(*this == other); }
	};

	//	キーの状態
	state get_key_state(key k, size_t pad_index = 0);

	//	キーの押し込み量
	float get_key_value(key k, size_t pad_index = 0);
}
