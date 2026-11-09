#include "Input/input_key.h"

namespace
{
#if defined(USE_IMGUI)
	//	@tool マウスを渡すと決めた ImGui のフレーム番号
	int mouse_pass_through_frame = -2;
#endif	//	defined(USE_IMGUI)

	bool is_keyboard_blocked()
	{
#if defined(USE_IMGUI)
		return ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
#else
		return false;
#endif
	}

	bool is_mouse_blocked()
	{
#if defined(USE_IMGUI)
		return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse && mouse_pass_through_frame < ImGui::GetFrameCount() - 1;
#else
		return false;
#endif
	}

	const gamepad::states* find_gamepad_state(size_t pad_index)
	{
		if (!gamepad::is_valid() || pad_index >= gamepad::max_count)
			return nullptr;
		return &gamepad::instance()->get_state(pad_index);
	}
}

namespace input
{
#if defined(USE_IMGUI)
	void pass_mouse_through_ui()
	{
		if (ImGui::GetCurrentContext())
			mouse_pass_through_frame = ImGui::GetFrameCount();
	}
#endif	//	defined(USE_IMGUI)

	state get_key_state(key k, size_t pad_index)
	{
		switch (k.device)
		{
			case devices::keyboard:
			{
				return keyboard::is_valid() && !is_keyboard_blocked() ? keyboard::instance()->get_state().is_key_state(static_cast<keyboard::key_states>(k.code)) : state::none;
			}
			case devices::mouse:
			{
				return mouse::is_valid() && !is_mouse_blocked() ? mouse::instance()->get_state().is_key_state(static_cast<mouse::key_states>(k.code)) : state::none;
			}
			case devices::mouse_axis:
			{
				return mouse::is_valid() && !is_mouse_blocked() ? mouse::instance()->get_axis_state(static_cast<mouse::axis_states>(k.code)) : state::none;
			}
			case devices::gamepad:
			{
				const gamepad::states* pad = find_gamepad_state(pad_index);
				return pad ? pad->is_key_state(static_cast<gamepad::key_states>(k.code)) : state::none;
			}
			case devices::gamepad_axis:
			{
				const gamepad::states* pad = find_gamepad_state(pad_index);
				return pad ? pad->get_axis_state(static_cast<gamepad::axis_states>(k.code)) : state::none;
			}
		}
		return state::none;
	}

	float get_key_value(key k, size_t pad_index)
	{
		switch (k.device)
		{
			case devices::mouse_axis:
			{
				return mouse::is_valid() && !is_mouse_blocked() ? mouse::instance()->get_axis(static_cast<mouse::axis_states>(k.code)) : 0.0f;
			}
			case devices::gamepad_axis:
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
