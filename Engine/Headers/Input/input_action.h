#pragma once

#include "input_key.h"
#include "input_layer.h"
#include "System/signals.h"

namespace input
{
	//	アクションフェーズ
	enum class action_phases : uint8_t
	{
		pressed,	//	押された
		released,	//	離された
		canceled,	//	押されたまま無効になった（レイヤーで止められた・外された）
	};

	//	アクションの通知に載せる内容
	struct action_event
	{
		action_phases phase;
		size_t pad_index;
	};

	//	入力アクション
	class action
	{
	public:
		action(std::string_view name, std::initializer_list<key> keys);
		action(const layer& owner, std::string_view name, std::initializer_list<key> keys);
		~action();
		action(const action&) = delete;
		action& operator=(const action&) = delete;

		state get_state(size_t pad_index = 0) const;
		bool is_pressed(size_t pad_index = 0) const;

		std::string_view get_name() const;
		const std::vector<key>& get_keys() const;

		[[nodiscard]] signals::connection subscribe(signals::broadcaster& target, std::function<void(const action_event&)> function,
			std::source_location location = std::source_location::current()) const;

	private:
		friend void update_actions(signals::broadcaster& target);

		std::string name;
		std::vector<key> keys;
		signals::topic<action_event> topic;
		const layer* owner = nullptr;
	};

	//	入力アクション更新
	void update_actions(signals::broadcaster& target);

	//	軸アクション
	class axis_action
	{
	public:
		struct mapping
		{
			key k;
			float scale = 1.0f;
		};

	public:
		axis_action(std::string_view name, std::initializer_list<mapping> mappings);
		axis_action(const layer& owner, std::string_view name, std::initializer_list<mapping> mappings);
		~axis_action();
		axis_action(const axis_action&) = delete;
		axis_action& operator=(const axis_action&) = delete;

		float get_value(size_t pad_index = 0) const;

		std::string_view get_name() const;
		const std::vector<mapping>& get_mappings() const;

	private:
		std::string name;
		std::vector<mapping> mappings;	
		const layer* owner = nullptr;
	};
}
