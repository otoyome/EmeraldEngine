#include "Input/input_key.h"

namespace
{
	const gamepad::states* find_gamepad_state(size_t pad_index)
	{
		if (!gamepad::is_valid() || pad_index >= gamepad::max_count)
			return nullptr;
		return &gamepad::instance()->get_state(pad_index);
	}
}

namespace input
{
	state get_key_state(key k, size_t pad_index)
	{
		switch (k.source)
		{
			case sources::keyboard:
			{
				return keyboard::is_valid() ? keyboard::instance()->get_state().is_key_state(static_cast<keyboard::key_states>(k.code)) : state::none;
			}
			case sources::mouse:
			{
				return mouse::is_valid() ? mouse::instance()->get_state().is_key_state(static_cast<mouse::key_states>(k.code)) : state::none;
			}
			case sources::mouse_axis:
			{
				return mouse::is_valid() ? mouse::instance()->get_axis_state(static_cast<mouse::axis_states>(k.code)) : state::none;
			}
			case sources::gamepad:
			{
				const gamepad::states* pad = find_gamepad_state(pad_index);
				return pad ? pad->is_key_state(static_cast<gamepad::key_states>(k.code)) : state::none;
			}
			case sources::gamepad_axis:
			{
				const gamepad::states* pad = find_gamepad_state(pad_index);
				return pad ? pad->get_axis_state(static_cast<gamepad::axis_states>(k.code)) : state::none;
			}
		}
		return state::none;
	}

	float get_key_value(key k, size_t pad_index)
	{
		switch (k.source)
		{
			case sources::mouse_axis:
			{
				return mouse::is_valid() ? mouse::instance()->get_axis(static_cast<mouse::axis_states>(k.code)) : 0.0f;
			}
			case sources::gamepad_axis:
			{
				const gamepad::states* pad = find_gamepad_state(pad_index);
				return pad ? pad->get_axis(static_cast<gamepad::axis_states>(k.code)) : 0.0f;
			}
			default:
			{
				const state current = get_key_state(k, pad_index);
				return current == state::down || current == state::hold ? 1.0f : 0.0f;
			}
		}
	}
}
