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

	struct action_record
	{
		input::action* target;
		input::state last_state = input::state::none;
	};

	std::vector<action_record>& action_records()
	{
		static std::vector<action_record> records;
		return records;
	}
}

namespace input
{
	action::action(std::string_view name, std::initializer_list<key> keys)
		: name(name), keys(keys), topic(this->name)
	{
		action_records().push_back({ this });
	}

	action::action(const layer& owner, std::string_view name, std::initializer_list<key> keys)
		: name(name), keys(keys), topic(this->name), owner(&owner)
	{
		action_records().push_back({ this });
		detail::add_layer_keys(owner, this->keys);
	}

	action::~action()
	{
		std::erase_if(action_records(), [this](const action_record& record) { return record.target == this; });
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

	signals::connection action::subscribe(signals::broadcaster& target, std::function<void(const action_event&)> function,
		std::source_location location) const
	{
		return target.subscribe(topic, std::move(function), location);
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

	void update_actions(signals::broadcaster& target)
	{
		std::vector<action_record>& records = action_records();

		for (size_t index = 0; index < records.size(); ++index)
		{
			action* current_action = records[index].target;
			const state last = records[index].last_state;
			const state current = current_action->get_state();
			records[index].last_state = current;

			const bool was_pressed = last == state::down || last == state::hold;
			if (current == state::down)
				target.publish(current_action->topic, { action_phases::pressed, 0 });
			else if (current == state::release && was_pressed)
				target.publish(current_action->topic, { action_phases::released, 0 });
			else if (current == state::none && was_pressed)
				target.publish(current_action->topic, { action_phases::canceled, 0 });
		}
	}
}
