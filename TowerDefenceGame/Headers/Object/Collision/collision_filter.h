#pragma once

#include <array>
#include <mutex>
#include <unordered_set>

#include "System/engine_config_data.h"

//	応答
using collision_response = engine_config_data::collision_data::responses;

//	コリジョンフィルター
class collision_filter
{
public:
	//	チャンネルの最大数
	static constexpr size_t max_channels = engine_config_data::collision_data::max_channels;

	std::string_view get_name() const;
	void set_name(std::string_view name);

	//	所属するチャンネルの番号
	size_t get_channel() const;
	void set_channel(size_t channel);

	//	チャンネルへの応答
	collision_response get_response(size_t channel) const;
	void set_response(size_t channel, collision_response response);

public:
	collision_filter();
	~collision_filter();

public:
	//	2つのフィルターの間の応答
	static collision_response get_response(const collision_filter& a, const collision_filter& b);

private:
	std::string name; // 名前
	size_t channel; // 所属するチャンネル
	std::array<collision_response, max_channels> responses; // チャンネルごとの応答
};

using collision_filter_ptr = std::shared_ptr<collision_filter>;

//	コリジョンフィルター
class collision_filter_manager
{
	SINGLETON(collision_filter_manager)

private:
	collision_filter_manager();
	~collision_filter_manager();

public:
	collision_filter_ptr search_filter(std::string_view name);
	void rebuild_filters();

	//	チャンネル名から番号を引く
	static size_t find_channel(std::string_view name);

private:
	//	初期値
	static void fill_default_config(engine_config_data::collision_data& data);

private:
	std::unordered_map<std::string, collision_filter_ptr> filters;
	std::unordered_set<std::string> missing_names;	//	見つからなかった名前
	std::mutex missing_mutex;						//	missing_names の排他
};
