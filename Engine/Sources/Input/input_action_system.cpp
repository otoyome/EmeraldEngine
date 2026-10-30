#include "Input/input_action_system.h"

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
	action_system::action_system()
		: layers(), actions(), axis_actions()
	{
	}

	action_system::~action_system()
	{
		_ASSERT_EXPR_A(actions.empty() && axis_actions.empty(), "input::action_system: 破棄時にアクションが残っています");
	}

	void action_system::push_layer(const layer& target)
	{
		layers.push(target);
	}

	void action_system::pop_layer(const layer& target)
	{
		layers.pop(target);
	}

	bool action_system::is_layer_active(const layer& target) const
	{
		return layers.is_active(target);
	}

	const layer_stack& action_system::get_layers() const
	{
		return layers;
	}

	void action_system::update(signals::broadcaster& target)
	{
		layers.update();

		//	受け手がアクションを作る・消すこともあるので、毎回大きさを確かめて添字で回す
		for (size_t index = 0; index < actions.size(); ++index)
		{
			action* current_action = actions[index].target;
			const state last = actions[index].last_state;
			const state current = current_action->get_state();
			actions[index].last_state = current;

			const bool was_pressed = last == state::down || last == state::hold;
			if (current == state::down)
				target.publish(current_action->topic, { action_phases::pressed, 0 });
			else if (current == state::release && was_pressed)
				target.publish(current_action->topic, { action_phases::released, 0 });
			else if (current == state::none && was_pressed)
				target.publish(current_action->topic, { action_phases::canceled, 0 });
		}

		//	軸は値が変わったときだけ配る（止められたときは 0 になる）
		for (size_t index = 0; index < axis_actions.size(); ++index)
		{
			axis_action* current_action = axis_actions[index].target;
			const float last = axis_actions[index].last_value;
			const float current = current_action->get_value();
			axis_actions[index].last_value = current;

			if (current != last)
				target.publish(current_action->topic, { current, 0 });
		}
	}

	void action_system::add_action(action* target)
	{
		actions.push_back({ target });
		if (target->owner)
			layers.add_keys(*target->owner, target->keys);
	}

	void action_system::remove_action(action* target)
	{
		std::erase_if(actions, [target](const action_record& record) { return record.target == target; });
		if (target->owner)
			layers.remove_keys(*target->owner, target->keys);
	}

	void action_system::add_axis_action(axis_action* target)
	{
		axis_actions.push_back({ target });
		if (target->owner)
			layers.add_keys(*target->owner, collect_keys(target->mappings));
	}

	void action_system::remove_axis_action(axis_action* target)
	{
		std::erase_if(axis_actions, [target](const axis_action_record& record) { return record.target == target; });
		if (target->owner)
			layers.remove_keys(*target->owner, collect_keys(target->mappings));
	}
}
