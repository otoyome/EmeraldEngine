#include "Input/gamepad.h"

#include <Xinput.h>

#pragma comment(lib, "xinput.lib")

namespace
{
	//	key_states と同じ並び
	static const WORD KeyMap[] =
	{
		//	十字キー
		XINPUT_GAMEPAD_DPAD_UP,
		XINPUT_GAMEPAD_DPAD_DOWN,
		XINPUT_GAMEPAD_DPAD_LEFT,
		XINPUT_GAMEPAD_DPAD_RIGHT,

		//	ボタン入力
		XINPUT_GAMEPAD_A,
		XINPUT_GAMEPAD_B,
		XINPUT_GAMEPAD_X,
		XINPUT_GAMEPAD_Y,
		XINPUT_GAMEPAD_LEFT_SHOULDER,
		XINPUT_GAMEPAD_RIGHT_SHOULDER,
		XINPUT_GAMEPAD_LEFT_THUMB,
		XINPUT_GAMEPAD_RIGHT_THUMB,
		XINPUT_GAMEPAD_START,
		XINPUT_GAMEPAD_BACK,

		//	LRトリガー
		0,	//	lt はトリガーの量で決める
		0,	//	rt はトリガーの量で決める
	};
	static_assert(std::size(KeyMap) == magic_enum::enum_count<gamepad::key_states>());

	//	未接続のパッドを調べ直す間隔
	constexpr uint32_t disconnected_retry_frames = 60;

	void apply_stick_dead_zone(SHORT x, SHORT y, SHORT dead_zone, float& out_x, float& out_y)
	{
		//	デッドゾーンを除いて 0～1 に詰め直す
		const float value_x = std::clamp(static_cast<float>(x) / 32767.0f, -1.0f, 1.0f);
		const float value_y = std::clamp(static_cast<float>(y) / 32767.0f, -1.0f, 1.0f);
		const float length = std::sqrt(value_x * value_x + value_y * value_y);
		const float threshold = static_cast<float>(dead_zone) / 32767.0f;
		if (length <= threshold)
		{
			out_x = 0.0f;
			out_y = 0.0f;
			return;
		}
		const float scale = (std::min(length, 1.0f) - threshold) / (1.0f - threshold) / length;
		out_x = value_x * scale;
		out_y = value_y * scale;
	}

	float apply_trigger_dead_zone(BYTE value)
	{
		const float threshold = static_cast<float>(XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		return std::clamp((static_cast<float>(value) - threshold) / (255.0f - threshold), 0.0f, 1.0f);
	}

	//	自プロセスの窓が前面か
	bool is_process_foreground()
	{
		HWND foreground = ::GetForegroundWindow();
		if (!foreground)
			return false;
		DWORD process_id = 0;
		::GetWindowThreadProcessId(foreground, &process_id);
		return process_id == ::GetCurrentProcessId();
	}
}

input::state gamepad::states::is_key_state(key_states state) const
{
	return key_state[static_cast<size_t>(state)];
}

float gamepad::states::get_axis(axis_states state) const
{
	return axis[static_cast<size_t>(state)];
}

input::state gamepad::states::get_axis_state(axis_states state) const
{
	return axis_key_state[static_cast<size_t>(state)];
}

const gamepad::states& gamepad::get_state(size_t index) const
{
	return pad_states[index];
}

void gamepad::set_vibration(size_t index, float low_frequency, float high_frequency)
{
	if (index >= max_count || !pad_states[index].connected)
		return;

	XINPUT_VIBRATION vibration = {};
	vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(low_frequency, 0.0f, 1.0f) * 65535.0f);
	vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(high_frequency, 0.0f, 1.0f) * 65535.0f);
	::XInputSetState(static_cast<DWORD>(index), &vibration);
}

gamepad::gamepad()
: pad_states()
, retry_counts()
{
	//	初回は全て調べる
	for (size_t index = 0; index < max_count; ++index)
	{
		retry_counts[index] = disconnected_retry_frames;
	}
}

gamepad::~gamepad()
{
	for (size_t index = 0; index < max_count; ++index)
	{
		set_vibration(index, 0.0f, 0.0f);
	}
}

void gamepad::update_state()
{
	const bool active = is_process_foreground();
	for (size_t index = 0; index < max_count; ++index)
	{
		states& state = pad_states[index];

		//	読めなかった時は全て離した扱い
		XINPUT_STATE input_state = {};
		bool read = false;
		if (state.connected || ++retry_counts[index] >= disconnected_retry_frames)
		{
			//	未接続のパッドは間隔を空けて調べる
			retry_counts[index] = 0;
			read = ::XInputGetState(static_cast<DWORD>(index), &input_state) == ERROR_SUCCESS;
		}
		state.connected = read;

		const XINPUT_GAMEPAD& pad = input_state.Gamepad;
		float* axis = state.axis;
		if (read && active)
		{
			apply_stick_dead_zone(pad.sThumbLX, pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE,
				axis[static_cast<size_t>(axis_states::left_x)], axis[static_cast<size_t>(axis_states::left_y)]);
			apply_stick_dead_zone(pad.sThumbRX, pad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE,
				axis[static_cast<size_t>(axis_states::right_x)], axis[static_cast<size_t>(axis_states::right_y)]);
			axis[static_cast<size_t>(axis_states::left_trigger)] = apply_trigger_dead_zone(pad.bLeftTrigger);
			axis[static_cast<size_t>(axis_states::right_trigger)] = apply_trigger_dead_zone(pad.bRightTrigger);
		}
		else
		{
			std::fill(std::begin(state.axis), std::end(state.axis), 0.0f);
		}

		const WORD buttons = read && active ? pad.wButtons : 0;
		for (size_t key = 0; key < magic_enum::enum_count<key_states>(); ++key)
		{
			bool pressed = (buttons & KeyMap[key]) != 0;
			if (static_cast<key_states>(key) == key_states::lt)
				pressed = state.get_axis(axis_states::left_trigger) >= input::pressed_threshold;
			else if (static_cast<key_states>(key) == key_states::rt)
				pressed = state.get_axis(axis_states::right_trigger) >= input::pressed_threshold;
			state.key_state[key] = input::next_state(state.key_state[key], pressed);
		}

		for (size_t key = 0; key < magic_enum::enum_count<axis_states>(); ++key)
		{
			state.axis_key_state[key] = input::next_state(state.axis_key_state[key], std::abs(state.axis[key]) >= input::pressed_threshold);
		}
	}
}
