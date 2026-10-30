#include "Input/input_layer.h"

namespace
{
	using namespace input;

	bool contains(const std::vector<key>& keys, key k)
	{
		return std::find(keys.begin(), keys.end(), k) != keys.end();
	}

	size_t get_pad_count(key k)
	{
		return k.device == devices::gamepad || k.device == devices::gamepad_axis ? gamepad::max_count : 1;
	}
}

namespace input
{
	layer::layer(std::string_view name, layer_blocks blocks)
		: name(name), blocks(blocks)
	{
	}

	std::string_view layer::get_name() const
	{
		return name;
	}

	layer_blocks layer::get_blocks() const
	{
		return blocks;
	}

	void layer_stack::push(const layer& target)
	{
		requests.push_back({ &target, true });
	}

	void layer_stack::pop(const layer& target)
	{
		requests.push_back({ &target, false });
	}

	bool layer_stack::is_active(const layer& target) const
	{
		return std::find(stack.begin(), stack.end(), &target) != stack.end();
	}

	void layer_stack::update()
	{
		for (const layer_request& request : requests)
		{
			stack.erase(std::remove(stack.begin(), stack.end(), request.target), stack.end());
			if (request.push)
				stack.push_back(request.target);
		}
		requests.clear();

		for (auto& [owner, s] : states)
		{
			std::erase_if(s.ignored_keys, [](const pad_key& ignored)
			{
				const state current = get_key_state(ignored.k, ignored.pad_index);
				return current == state::none || current == state::down;
			});

			s.was_active = s.active;
			s.was_blocked_all = s.blocked_all;
			s.was_blocked_keys.swap(s.blocked_keys);
			s.active = false;
			s.blocked_all = false;
			s.blocked_keys.clear();
		}

		//	下流へ止めるキーを積み上げる
		bool block_all = false;
		std::vector<key> block_keys;
		for (auto it = stack.rbegin(); it != stack.rend(); ++it)
		{
			layer_state& s = states[*it];
			s.active = true;
			s.blocked_all = block_all;
			s.blocked_keys = block_keys;

			switch ((*it)->get_blocks())
			{
				case layer_blocks::all:
				{
					block_all = true;
					break;
				}
				case layer_blocks::used_keys:
				{
					block_keys.insert(block_keys.end(), s.keys.begin(), s.keys.end());
					break;
				}
				default:
				{
					break;
				}
			}
		}

		//	使えるようになったキーが押されたままなら、離されるまで無視する
		for (auto& [owner, s] : states)
		{
			for (const key& k : s.keys)
			{
				const bool was_usable = s.was_active && !s.was_blocked_all && !contains(s.was_blocked_keys, k);
				const bool usable = s.active && !s.blocked_all && !contains(s.blocked_keys, k);
				if (was_usable || !usable)
					continue;

				for (size_t pad_index = 0; pad_index < get_pad_count(k); ++pad_index)
				{
					if (get_key_state(k, pad_index) != state::none && !is_ignored(s, k, pad_index))
						s.ignored_keys.push_back({ k, pad_index });
				}
			}
		}
	}

	bool layer_stack::is_key_blocked(const layer& target, key k, size_t pad_index) const
	{
		const auto it = states.find(&target);
		if (it == states.end())
			return false;

		const layer_state& s = it->second;
		if (s.blocked_all || contains(s.blocked_keys, k))
			return true;
		return is_ignored(s, k, get_pad_count(k) > 1 ? pad_index : 0);
	}

	void layer_stack::add_keys(const layer& target, const std::vector<key>& keys)
	{
		std::vector<key>& layer_keys = states[&target].keys;
		layer_keys.insert(layer_keys.end(), keys.begin(), keys.end());
	}

	void layer_stack::remove_keys(const layer& target, const std::vector<key>& keys)
	{
		std::vector<key>& layer_keys = states[&target].keys;
		for (const key& k : keys)
		{
			const auto it = std::find(layer_keys.begin(), layer_keys.end(), k);
			if (it != layer_keys.end())
				layer_keys.erase(it);
		}
	}

	bool layer_stack::is_ignored(const layer_state& s, key k, size_t pad_index)
	{
		return std::any_of(s.ignored_keys.begin(), s.ignored_keys.end(),
						   [&](const pad_key& ignored) { return ignored.k == k && ignored.pad_index == pad_index; });
	}
}
