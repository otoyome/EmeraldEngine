#pragma once

#include "input_layer.h"
#include "Core/signals.h"

namespace input
{
	class action;
	class axis_action;

	//	ì¸óÕÉAÉNÉVÉáÉìä«óù
	class action_system
	{
		SINGLETON(action_system)

		friend class action;
		friend class axis_action;

		struct action_record
		{
			action* target;
			state last_state = state::none;
		};

		struct axis_action_record
		{
			axis_action* target;
			float last_value = 0.0f;
		};

	public:
		const layer_stack& get_layers() const;

	private:
		action_system();
		~action_system();

	public:
		void update(core::signals::broadcaster& target);

		void push_layer(const layer& target);
		void pop_layer(const layer& target);
		bool is_layer_active(const layer& target) const;

	private:
		void add_action(action* target);
		void remove_action(action* target);
		void add_axis_action(axis_action* target);
		void remove_axis_action(axis_action* target);

	private:
		layer_stack layers;
		std::vector<action_record> actions;
		std::vector<axis_action_record> axis_actions;
	};
}
