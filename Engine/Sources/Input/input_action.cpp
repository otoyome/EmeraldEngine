#include "Input/input_action.h"

#include "Input/input_action_system.h"

namespace input
{
	action::action(std::string_view name, std::initializer_list<key> keys)
		: action(nullptr, name, keys)
	{
	}

	action::action(const layer& owner, std::string_view name, std::initializer_list<key> keys)
		: action(&owner, name, keys)
	{
	}

	action::action(const layer* owner, std::string_view name, std::initializer_list<key> keys)
		: name(name), keys(keys), topic(name), owner(owner)
	{
		_ASSERT_EXPR_A(action_system::is_valid(), "input::action: action_system ‚æ‚èæ‚Éì‚ç‚ê‚Ä‚¢‚Ü‚·");
		if (action_system::is_valid())
			action_system::instance()->add_action(this);
	}

	action::~action()
	{
		if (action_system::is_valid())
			action_system::instance()->remove_action(this);
	}

	state action::get_state(size_t pad_index) const
	{
		const layer_stack* layers = nullptr;
		if (owner)
		{
			if (!action_system::is_valid() || !action_system::instance()->is_layer_active(*owner))
				return state::none;
			layers = &action_system::instance()->get_layers();
		}

		bool pressed = false;
		bool was_pressed = false;
		for (const key& k : keys)
		{
			if (layers && layers->is_key_blocked(*owner, k, pad_index))
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

	core::signals::connection action::subscribe(core::signals::broadcaster& target, std::function<void(const action_event&)> function,
		std::source_location location) const
	{
		return target.subscribe(topic, std::move(function), location);
	}

	axis_action::axis_action(std::string_view name, std::initializer_list<mapping> mappings)
		: axis_action(nullptr, name, mappings)
	{
	}

	axis_action::axis_action(const layer& owner, std::string_view name, std::initializer_list<mapping> mappings)
		: axis_action(&owner, name, mappings)
	{
	}

	axis_action::axis_action(const layer* owner, std::string_view name, std::initializer_list<mapping> mappings)
		: name(name), mappings(mappings), topic(name), owner(owner)
	{
		_ASSERT_EXPR_A(action_system::is_valid(), "input::axis_action: action_system ‚æ‚èæ‚Éì‚ç‚ê‚Ä‚¢‚Ü‚·");
		if (action_system::is_valid())
			action_system::instance()->add_axis_action(this);
	}

	axis_action::~axis_action()
	{
		if (action_system::is_valid())
			action_system::instance()->remove_axis_action(this);
	}

	float axis_action::get_value(size_t pad_index) const
	{
		const layer_stack* layers = nullptr;
		if (owner)
		{
			if (!action_system::is_valid() || !action_system::instance()->is_layer_active(*owner))
				return 0.0f;
			layers = &action_system::instance()->get_layers();
		}

		float value = 0.0f;
		for (const mapping& mapping : mappings)
		{
			if (layers && layers->is_key_blocked(*owner, mapping.k, pad_index))
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

	core::signals::connection axis_action::subscribe(core::signals::broadcaster& target, std::function<void(const axis_event&)> function,
		std::source_location location) const
	{
		return target.subscribe(topic, std::move(function), location);
	}
}
