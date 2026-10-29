#pragma once

namespace input
{
	enum class state
	{
		none,
		down,
		hold,
		release,
	};

	//	—Ê‚ğ‚Â“ü—Í‚ğ‰Ÿ‰º‚Æ‚İ‚È‚·—Ê
	constexpr float pressed_threshold = 0.5f;

	//	Œ»İ‚Ìó‘Ô‚©‚çŸ‚Ìó‘Ô‚ğ‹‚ß‚é
	inline state next_state(state current, bool pressed)
	{
		if (pressed)
			return current == state::none || current == state::release ? state::down : state::hold;
		return current == state::down || current == state::hold ? state::release : state::none;
	}
}
