#include "Input/keyboard.h"

namespace
{
	static const int KeyMap[] =
	{
		'A',
		'B',
		'C',
		'D',
		'E',
		'F',
		'G',
		'H',
		'I',
		'J',
		'K',
		'L',
		'M',
		'N',
		'O',
		'P',
		'Q',
		'R',
		'S',
		'T',
		'U',
		'V',
		'W',
		'X',
		'Y',
		'Z',

		VK_SHIFT,
		VK_LSHIFT,
		VK_RSHIFT,
		VK_CONTROL,
		VK_LCONTROL,
		VK_RCONTROL,
		VK_MENU,
		VK_SPACE,

		VK_F1,
		VK_F2,
		VK_F3,
		VK_F4,
		VK_F5,
		VK_F6,
		VK_F7,
		VK_F8,
		VK_F9,
		VK_F10,
		VK_F11,
		VK_F12,
		VK_OEM_3,

		'0',
		'1',
		'2',
		'3',
		'4',
		'5',
		'6',
		'7',
		'8',
		'9',

		VK_LMENU,
		VK_RMENU,
		VK_LWIN,
		VK_RWIN,
		VK_APPS,

		VK_RETURN,
		VK_ESCAPE,
		VK_TAB,
		VK_BACK,
		VK_CAPITAL,

		VK_UP,
		VK_DOWN,
		VK_LEFT,
		VK_RIGHT,

		VK_INSERT,
		VK_DELETE,
		VK_HOME,
		VK_END,
		VK_PRIOR,
		VK_NEXT,

		VK_NUMPAD0,
		VK_NUMPAD1,
		VK_NUMPAD2,
		VK_NUMPAD3,
		VK_NUMPAD4,
		VK_NUMPAD5,
		VK_NUMPAD6,
		VK_NUMPAD7,
		VK_NUMPAD8,
		VK_NUMPAD9,
		VK_ADD,
		VK_SUBTRACT,
		VK_MULTIPLY,
		VK_DIVIDE,
		VK_DECIMAL,
		VK_NUMLOCK,

		VK_OEM_MINUS,
		VK_OEM_PLUS,
		VK_OEM_COMMA,
		VK_OEM_PERIOD,
		VK_OEM_1,
		VK_OEM_2,
		VK_OEM_4,
		VK_OEM_5,
		VK_OEM_6,
		VK_OEM_7,
		VK_OEM_102,

		VK_CONVERT,
		VK_NONCONVERT,

		VK_SCROLL,
		VK_PAUSE,
	};

	static_assert(std::size(KeyMap) == magic_enum::enum_count<keyboard::key_states>(), "keyboard::key_states Ç∆ KeyMap ÇÃêîÇ™çáÇ¡ÇƒÇ¢Ç»Ç¢");
	static_assert(static_cast<size_t>(keyboard::key_states::pause) + 1 == std::size(KeyMap), "keyboard::key_states ÇÃç≈å„ÇÃílÇ™ KeyMap Ç∆çáÇ¡ÇƒÇ¢Ç»Ç¢");
}

input::state keyboard::states::is_key_state(key_states state) const
{
	return key_state[static_cast<size_t>(state)];
}

void keyboard::states::set_key_state(key_states state, input::state flag)
{
	key_state[static_cast<size_t>(state)] = flag;
}

const keyboard::states& keyboard::get_state() const
{
	return state;
}

void keyboard::set_state(const states& state)
{
	this->state = state;
}


keyboard::keyboard()
{
}

keyboard::~keyboard()
{
}

void keyboard::update_state()
{
	BYTE input_data[256];
	if (!input::is_process_foreground() || !::GetKeyboardState(input_data))
	{
		memset(input_data, 0, sizeof(input_data));
	}
	for (size_t index = 0; index < magic_enum::enum_count<key_states>(); ++index)
	{
		state.key_state[index] = input::next_state(state.key_state[index], (input_data[KeyMap[index]] & 0x80) != 0);
	}
}
