#include "System/game_config_data.h"

#include "System/console_manager.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/map.hpp"

#include <fstream>
#include <map>
#include <string>

namespace
{
	static constexpr std::string_view file_path = "game_config.ini";
}

game_config_data::game_config_data()
{
	std::map<std::string, std::string> variables;

	std::ifstream istream(file_path.data());
	if (!istream.is_open())
	{
		DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Open Failed GameConfig[%s]", file_path.data());
		return;
	}

	try
	{
		cereal::JSONInputArchive archive(istream);
		archive(CEREAL_NVP(variables));
	}
	catch (...)
	{
		DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Load Failed GameConfig[%s]", file_path.data());
		return;
	}
	DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Load Succeeded GameConfig[%s]", file_path.data());

	//	•Û‘¶‚³‚ê‚Ä‚¢‚é•Ï”’l‚ð”½‰f
	auto& manager = console_manager::instance();
	for (auto& [name, value] : variables)
	{
		auto variable = manager.find_variable(name);
		if (variable && variable->is_flag(console_flags::user_setting))
			variable->from_string(value);
	}
}

game_config_data::~game_config_data()
{
	auto& manager = console_manager::instance();

	std::map<std::string, std::string> variables;
	for (auto* object : manager.collect(""))
	{
		if (object->is_variable() && object->is_flag(console_flags::user_setting))
			variables.insert_or_assign(object->get_name(), static_cast<const console_variable_base*>(object)->to_string());
	}

	std::ofstream ostream(file_path.data());
	if (!ostream.is_open())
	{
		DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Open Failed GameConfig[%s]", file_path.data());
		return;
	}

	try
	{
		cereal::JSONOutputArchive archive(ostream);
		archive(CEREAL_NVP(variables));
	}
	catch (...)
	{
		DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Save Failed GameConfig[%s]", file_path.data());
		return;
	}
	DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Save Succeeded GameConfig[%s]", file_path.data());
}
