#pragma once

#include "input.h"

struct SDL_Gamepad;

//	ゲームパッド
class gamepad
{
	SINGLETON(gamepad)

public:
	static constexpr size_t max_count = 4;

	enum class key_states
	{
		//	十字キー
		up,
		down,
		left,
		right,

		//	ボタン入力
		a,
		b,
		x,
		y,
		lb,
		rb,
		left_stick,
		right_stick,
		start,
		back,

		//	LRトリガー
		lt,
		rt,
	};

	//	量を持つ入力
	enum class axis_states
	{
		left_x,
		left_y,
		right_x,
		right_y,
		left_trigger,
		right_trigger,
	};

	struct states
	{
		bool connected = false;
		input::state key_state[magic_enum::enum_count<key_states>()] = {};
		float axis[magic_enum::enum_count<axis_states>()] = {};
		input::state axis_key_state[magic_enum::enum_count<axis_states>()] = {};	//	軸の量が押下とみなす量を超えているかの状態

		input::state is_key_state(key_states state) const;
		float get_axis(axis_states state) const;
		input::state get_axis_state(axis_states state) const;
	};

public:
	const states& get_state(size_t index) const;
	void set_vibration(size_t index, float low_frequency, float high_frequency);
	std::string_view get_name(size_t index) const;

private:
	gamepad();
	~gamepad();

public:
	void update_state();

private:
	struct slot
	{
		SDL_Gamepad* handle = nullptr;
		uint32_t id = 0;
		std::string name;
	};

	void update_slots();

private:
	states pad_states[max_count];
	slot slots[max_count];
	bool initialized;
};

ENUM_ATTR_BITFLAG(gamepad::key_states);
