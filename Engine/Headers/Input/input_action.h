#pragma once

#include "input_key.h"
#include "input_layer.h"
#include "Core/signals.h"

namespace input
{
	class action_system;

	//	アクションフェーズ
	enum class action_phases : uint8_t
	{
		pressed,	//	押された
		released,	//	離された
		canceled,	//	押されたまま無効になった(レイヤーで止められた・外された)
	};

	//	アクションイベント
	struct action_event
	{
		action_phases phase;
		size_t pad_index;
	};

	//	軸アクションイベント
	struct axis_event
	{
		float value;
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

		[[nodiscard]] core::signals::connection subscribe(core::signals::broadcaster& target, std::function<void(const action_event&)> function,
			std::source_location location = std::source_location::current()) const;

	private:
		friend class action_system;

		action(const layer* owner, std::string_view name, std::initializer_list<key> keys);

		std::string_view name;
		std::vector<key> keys;
		core::signals::topic<action_event> topic;
		const layer* owner = nullptr;
	};

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

		[[nodiscard]] core::signals::connection subscribe(core::signals::broadcaster& target, std::function<void(const axis_event&)> function,
			std::source_location location = std::source_location::current()) const;

	private:
		friend class action_system;

		axis_action(const layer* owner, std::string_view name, std::initializer_list<mapping> mappings);

		std::string_view name;
		std::vector<mapping> mappings;
		core::signals::topic<axis_event> topic;
		const layer* owner = nullptr;
	};
}
