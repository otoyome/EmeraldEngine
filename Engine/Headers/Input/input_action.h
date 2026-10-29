#pragma once

#include "input_key.h"
#include "input_layer.h"

namespace input
{
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

	private:
		std::string name;
		std::vector<key> keys;
		const layer* owner = nullptr;	//	所属レイヤー。nullptr ならいつでも有効
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

	private:
		std::string name;
		std::vector<mapping> mappings;	
		const layer* owner = nullptr;		//	所属レイヤー。nullptr ならいつでも有効
	};
}
