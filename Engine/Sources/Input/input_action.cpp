#include "Input/input_action.h"

namespace
{
	std::vector<input::key> collect_keys(const std::vector<input::axis_action::mapping>& mappings)
	{
		std::vector<input::key> keys;
		for (const input::axis_action::mapping& mapping : mappings)
		{
			keys.push_back(mapping.k);
		}
		return keys;
	}
}

namespace input
{
	action::action(std::string_view name, std::initializer_list<key> keys)
		: name(name), keys(keys)
	{
	}

	action::action(const layer& owner, std::string_view name, std::initializer_list<key> keys)
		: name(name), keys(keys), owner(&owner)
	{
		detail::add_layer_keys(owner, this->keys);
	}

	action::~action()
	{
		if (owner)
			detail::remove_layer_keys(*owner, keys);
	}

	state action::get_state(size_t pad_index) const
	{
		if (owner && !is_layer_active(*owner))
			return state::none;

		bool pressed = false;
		bool was_pressed = false;
		for (const key& k : keys)
		{
			if (owner && is_key_blocked(*owner, k, pad_index))
				continue;
			const state current = get_key_state(k, pad_index);
			pressed = pressed || current == state::down || current == state::hold;
			was_pressed = was_pressed || current == state::hold || current == state::release;
		}
		return next_state(was_pressed ? state::hold : state::none, pressed);
	}

	bool action::is_pressed(size_t pad_index) const
	{
		const state current = get_state(pad_index);
		return current == state::down || current == state::hold;
	}

	std::string_view action::get_name() const
	{
		return name;
	}

	const std::vector<key>& action::get_keys() const
	{
		return keys;
	}

	axis_action::axis_action(std::string_view name, std::initializer_list<mapping> mappings)
		: name(name), mappings(mappings)
	{
	}

	axis_action::axis_action(const layer& owner, std::string_view name, std::initializer_list<mapping> mappings)
		: name(name), mappings(mappings), owner(&owner)
	{
		detail::add_layer_keys(owner, collect_keys(this->mappings));
	}

	axis_action::~axis_action()
	{
		if (owner)
			detail::remove_layer_keys(*owner, collect_keys(mappings));
	}

	float axis_action::get_value(size_t pad_index) const
	{
		if (owner && !is_layer_active(*owner))
			return 0.0f;

		float value = 0.0f;
		for (const mapping& mapping : mappings)
		{
			if (owner && is_key_blocked(*owner, mapping.k, pad_index))
				continue;
			value += get_key_value(mapping.k, pad_index) * mapping.scale;
		}
		return value;
	}

	std::string_view axis_action::get_name() const
	{
		return name;
	}

	const std::vector<axis_action::mapping>& axis_action::get_mappings() const
	{
		return mappings;
	}
}
