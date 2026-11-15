#include "Object/Collision/collision_filter.h"

std::string_view collision_filter::get_name() const
{
	return name;
}

void collision_filter::set_name(std::string_view name)
{
	this->name = name;
}

size_t collision_filter::get_channel() const
{
	return channel;
}

void collision_filter::set_channel(size_t channel)
{
	this->channel = channel < max_channels ? channel : 0;
}

collision_response collision_filter::get_response(size_t channel) const
{
	return channel < max_channels ? responses[channel] : collision_response::ignore;
}

void collision_filter::set_response(size_t channel, collision_response response)
{
	if (channel < max_channels)
		responses[channel] = response;
}

collision_response collision_filter::get_response(const collision_filter& a, const collision_filter& b)
{
	return std::min(a.get_response(b.channel), b.get_response(a.channel));
}

collision_filter::collision_filter()
: channel(0)
{
	responses.fill(collision_response::block);
}

collision_filter::~collision_filter()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
collision_filter_manager::collision_filter_manager()
{
	//	@tool エンジン設定からフィルターを作る
	rebuild_filters();
}

collision_filter_manager::~collision_filter_manager()
{
}

bool collision_filter_manager::register_filter(collision_filter_ptr& filter)
{
	std::string filter_name = std::string(filter->get_name());
	auto it = filters.find(filter_name);
	if (it != filters.end())
		return false;

	filters.insert_or_assign(filter_name, filter);
	return true;
}

bool collision_filter_manager::unregister_filter(collision_filter_ptr& filter)
{
	return unregister_filter(filter->get_name());
}

bool collision_filter_manager::unregister_filter(std::string_view name)
{
	std::string filter_name = std::string(name);
	auto it = filters.find(filter_name);
	if (it == filters.end())
		return false;

	filters.erase(it);
	return true;
}

collision_filter_ptr collision_filter_manager::search_filter(std::string_view name)
{
	std::string filter_name = std::string(name);
	auto it = filters.find(filter_name);
	if (it == filters.end())
	{
		//	@tool 名前が見つからない時は default を使い、警告を名前ごとに1回だけ出す
		{
			std::lock_guard<std::mutex> lock(missing_mutex);
			if (missing_names.emplace(filter_name).second)
				DEBUG_LOG_CATEGORY(logger::categorys::system, "Collision Filter Not Found[%s]", filter_name.c_str());
		}
		it = filters.find("default");
	}

	return it->second;
}

void collision_filter_manager::rebuild_filters()
{
	engine_config_data::collision_data& data = engine_config_data::instance()->collision;
	if (data.channels.empty())
		fill_default_config(data);

	auto apply_profile = [&](const engine_config_data::collision_data::profile_data* profile, std::string_view name) -> collision_filter_ptr
	{
		//	名前が同じフィルターは作り直さず書き換える
		auto it = filters.find(std::string(name));
		collision_filter_ptr filter = it != filters.end() ? it->second : std::make_shared<collision_filter>();
		filter->set_name(name);

		size_t channel = 0;
		if (profile)
		{
			channel = find_channel(profile->object_type);
			if (channel >= collision_filter::max_channels)
			{
				DEBUG_LOG_CATEGORY(logger::categorys::system, "Collision Channel Not Found[%s] Profile[%s]", profile->object_type.c_str(), profile->name.c_str());
				channel = 0;
			}
		}
		filter->set_channel(channel);

		//	書いていないチャンネルは当たらない
		for (size_t index = 0; index < collision_filter::max_channels; ++index)
			filter->set_response(index, collision_response::ignore);
		if (profile)
		{
			for (const engine_config_data::collision_data::response_data& response : profile->channel_responses)
			{
				const size_t index = find_channel(response.channel);
				if (index < collision_filter::max_channels)
					filter->set_response(index, response.response);
			}
		}
		return filter;
	};

	std::unordered_map<std::string, collision_filter_ptr> rebuilt;
	for (const engine_config_data::collision_data::profile_data& profile : data.profiles)
	{
		if (profile.name.empty() || rebuilt.find(profile.name) != rebuilt.end())
		{
			DEBUG_LOG_CATEGORY(logger::categorys::system, "Collision Profile Skipped[%s]", profile.name.c_str());
			continue;
		}
		rebuilt.emplace(profile.name, apply_profile(&profile, profile.name));
	}

	//	default は必ず置く
	if (rebuilt.find("default") == rebuilt.end())
		rebuilt.emplace("default", apply_profile(nullptr, "default"));

	filters.swap(rebuilt);

	std::lock_guard<std::mutex> lock(missing_mutex);
	missing_names.clear();
}

size_t collision_filter_manager::find_channel(std::string_view name)
{
	const std::vector<engine_config_data::collision_data::channel_data>& channels = engine_config_data::instance()->collision.channels;
	for (size_t index = 0; index < channels.size() && index < collision_filter::max_channels; ++index)
	{
		if (channels[index].name == name)
			return index;
	}
	return collision_filter::max_channels;
}

void collision_filter_manager::fill_default_config(engine_config_data::collision_data& data)
{
	data.channels.clear();
	data.profiles.clear();

	const char* channel_names[] = { "player", "player_attack", "enemy", "enemy_attack", "stage", "invisible", "camera", "ik" };
	for (const char* name : channel_names)
		data.channels.emplace_back().name = name;

	auto add_profile = [&data](const char* name, const char* object_type, std::initializer_list<const char*> blocks)
	{
		engine_config_data::collision_data::profile_data& profile = data.profiles.emplace_back();
		profile.name = name;
		profile.object_type = object_type;
		for (const char* channel : blocks)
		{
			engine_config_data::collision_data::response_data& response = profile.channel_responses.emplace_back();
			response.channel = channel;
			response.response = collision_response::block;
		}
	};
	add_profile("default", "player", { "player", "player_attack", "enemy", "enemy_attack", "stage", "invisible", "camera", "ik" });
	add_profile("player", "player", { "enemy", "enemy_attack", "stage" });
	add_profile("player_attack", "player_attack", { "enemy", "stage" });
	add_profile("enemy", "enemy", { "player", "player_attack", "stage" });
	add_profile("enemy_attack", "enemy_attack", { "player", "stage" });
	add_profile("stage", "stage", { "player", "player_attack", "enemy", "enemy_attack", "stage", "invisible", "camera", "ik" });
	add_profile("invisible", "invisible", { "stage" });
	add_profile("camera", "camera", { "stage" });
	add_profile("ik", "ik", { "stage" });
}
