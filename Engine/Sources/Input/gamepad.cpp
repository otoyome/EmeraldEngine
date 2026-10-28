#include "Input/gamepad.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hints.h>

#pragma comment(lib, "SDL3.lib")

namespace
{
	//	key_states と同じ並び
	static const SDL_GamepadButton KeyMap[] =
	{
		//	十字キー
		SDL_GAMEPAD_BUTTON_DPAD_UP,
		SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		SDL_GAMEPAD_BUTTON_DPAD_LEFT,
		SDL_GAMEPAD_BUTTON_DPAD_RIGHT,

		//	ボタン入力
		SDL_GAMEPAD_BUTTON_SOUTH,
		SDL_GAMEPAD_BUTTON_EAST,
		SDL_GAMEPAD_BUTTON_WEST,
		SDL_GAMEPAD_BUTTON_NORTH,
		SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
		SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
		SDL_GAMEPAD_BUTTON_LEFT_STICK,
		SDL_GAMEPAD_BUTTON_RIGHT_STICK,
		SDL_GAMEPAD_BUTTON_START,
		SDL_GAMEPAD_BUTTON_BACK,

		//	LRトリガー
		SDL_GAMEPAD_BUTTON_INVALID,	//	lt はトリガーの量で決める
		SDL_GAMEPAD_BUTTON_INVALID,	//	rt はトリガーの量で決める
	};
	static_assert(std::size(KeyMap) == magic_enum::enum_count<gamepad::key_states>());

	//	デッドゾーン
	constexpr int left_stick_dead_zone = 7849;
	constexpr int right_stick_dead_zone = 8689;
	constexpr float trigger_dead_zone = 30.0f / 255.0f;

	void apply_stick_dead_zone(Sint16 x, Sint16 y, int dead_zone, float& out_x, float& out_y)
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

	float apply_trigger_dead_zone(Sint16 value)
	{
		const float normalized = std::clamp(static_cast<float>(value) / 32767.0f, 0.0f, 1.0f);
		return std::clamp((normalized - trigger_dead_zone) / (1.0f - trigger_dead_zone), 0.0f, 1.0f);
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

std::string_view gamepad::get_name(size_t index) const
{
	return index < max_count ? std::string_view(slots[index].name) : std::string_view();
}

void gamepad::set_vibration(size_t index, float low_frequency, float high_frequency)
{
	if (index >= max_count || !slots[index].handle)
		return;

	//	SDL の上限（約65秒）で止まる
	const Uint16 low = static_cast<Uint16>(std::clamp(low_frequency, 0.0f, 1.0f) * 65535.0f);
	const Uint16 high = static_cast<Uint16>(std::clamp(high_frequency, 0.0f, 1.0f) * 65535.0f);
	SDL_RumbleGamepad(slots[index].handle, low, high, UINT32_MAX);
}

gamepad::gamepad()
: pad_states()
, slots()
, initialized(false)
{
	//	ゲームパッドだけを初期化する
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	initialized = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
	if (!initialized)
	{
		DEBUG_LOG_CATEGORY(logger::categorys::system, "SDL_InitSubSystem(SDL_INIT_GAMEPAD) failed: %s", SDL_GetError());
		return;
	}
	SDL_SetGamepadEventsEnabled(false);
	SDL_SetJoystickEventsEnabled(false);
}

gamepad::~gamepad()
{
	for (size_t index = 0; index < max_count; ++index)
	{
		set_vibration(index, 0.0f, 0.0f);
		if (slots[index].handle)
			SDL_CloseGamepad(slots[index].handle);
		slots[index] = slot();
	}
	if (initialized)
	{
		SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
		SDL_Quit();
	}
}

void gamepad::update_slots()
{
	SDL_UpdateGamepads();

	int count = 0;
	SDL_JoystickID* ids = SDL_GetGamepads(&count);
	const auto connected = [ids, count](SDL_JoystickID id)
	{
		return ids && std::find(ids, ids + count, id) != ids + count;
	};

	//	外れたパッドを閉じる
	for (auto& entry : slots)
	{
		if (entry.handle && !connected(entry.id))
		{
			SDL_CloseGamepad(entry.handle);
			entry = slot();
		}
	}

	//	新しいパッドを空いている番号に先着順で割り当てる
	for (int index = 0; ids && index < count; ++index)
	{
		const SDL_JoystickID id = ids[index];
		if (std::any_of(std::begin(slots), std::end(slots), [id](const slot& entry) { return entry.handle && entry.id == id; }))
			continue;

		auto free_slot = std::find_if(std::begin(slots), std::end(slots), [](const slot& entry) { return !entry.handle; });
		if (free_slot == std::end(slots))
			break;

		SDL_Gamepad* handle = SDL_OpenGamepad(id);
		if (!handle)
			continue;
		free_slot->handle = handle;
		free_slot->id = id;
		const char* name = SDL_GetGamepadName(handle);
		free_slot->name = name ? name : "";
	}
	SDL_free(ids);
}

void gamepad::update_state()
{
	if (initialized)
		update_slots();

	const bool active = is_process_foreground();
	for (size_t index = 0; index < max_count; ++index)
	{
		states& state = pad_states[index];
		SDL_Gamepad* handle = slots[index].handle;

		//	読めなかった時は全て離した扱い
		const bool read = handle && SDL_GamepadConnected(handle);
		state.connected = read;

		float* axis = state.axis;
		if (read && active)
		{
			//	SDL は下が正なので y を反転して上を正にする
			float y = 0.0f;
			apply_stick_dead_zone(SDL_GetGamepadAxis(handle, SDL_GAMEPAD_AXIS_LEFTX), SDL_GetGamepadAxis(handle, SDL_GAMEPAD_AXIS_LEFTY), left_stick_dead_zone,
				axis[static_cast<size_t>(axis_states::left_x)], y);
			axis[static_cast<size_t>(axis_states::left_y)] = -y;
			apply_stick_dead_zone(SDL_GetGamepadAxis(handle, SDL_GAMEPAD_AXIS_RIGHTX), SDL_GetGamepadAxis(handle, SDL_GAMEPAD_AXIS_RIGHTY), right_stick_dead_zone,
				axis[static_cast<size_t>(axis_states::right_x)], y);
			axis[static_cast<size_t>(axis_states::right_y)] = -y;
			axis[static_cast<size_t>(axis_states::left_trigger)] = apply_trigger_dead_zone(SDL_GetGamepadAxis(handle, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
			axis[static_cast<size_t>(axis_states::right_trigger)] = apply_trigger_dead_zone(SDL_GetGamepadAxis(handle, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
		}
		else
		{
			std::fill(std::begin(state.axis), std::end(state.axis), 0.0f);
		}

		for (size_t key = 0; key < magic_enum::enum_count<key_states>(); ++key)
		{
			bool pressed = read && active && KeyMap[key] != SDL_GAMEPAD_BUTTON_INVALID && SDL_GetGamepadButton(handle, KeyMap[key]);
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
