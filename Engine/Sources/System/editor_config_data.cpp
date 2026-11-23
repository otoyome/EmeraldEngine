#include "System/editor_config_data.h"

#if defined(USE_IMGUI)
#include "System/console_manager.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/map.hpp"

namespace
{
	//	エディター設定情報
	static constexpr std::string_view file_path = "editor_config.ini";
}

editor_config_data::editor_config_data()
{
	std::map<std::string, std::string> variables;
	std::map<std::string, std::string> bindings;

	bool loaded = false;
	std::ifstream istream(file_path.data());
	if (istream.is_open())
	{
		try
		{
			cereal::JSONInputArchive archive(istream);
			archive(CEREAL_NVP(variables), CEREAL_NVP(bindings));
			loaded = true;
			DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Load Succeeded EditorConfig[%s]", file_path.data());
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Load Failed EditorConfig[%s]", file_path.data());
		}
	}
	else
	{
		DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Open Failed EditorConfig[%s]", file_path.data());
	}

	auto& manager = console_manager::instance();

	//	保存されている変数値を反映する（persist 属性の物のみ）
	for (auto& [name, value] : variables)
	{
		auto variable = manager.find_variable(name);
		if (variable && variable->is_flag(console_flags::persist))
			variable->from_string(value);
	}

	//	キー割り当て
	if (loaded)
	{
		manager.clear_bindings();
		for (auto& [key, command] : bindings)
			manager.bind(key, command);
	}
	else
	{
		apply_default_bindings();
	}
}

editor_config_data::~editor_config_data()
{
	auto& manager = console_manager::instance();

	std::map<std::string, std::string> variables;
	for (auto* object : manager.collect(""))
	{
		if (object->is_variable() && object->is_flag(console_flags::persist))
			variables.insert_or_assign(object->get_name(), static_cast<const console_variable_base*>(object)->to_string());
	}
	std::map<std::string, std::string> bindings = manager.get_bindings();

	std::ofstream ostream(file_path.data());
	if (!ostream.is_open())
	{
		DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Open Failed EditorConfig[%s]", file_path.data());
		return;
	}

	try
	{
		cereal::JSONOutputArchive archive(ostream);
		archive(CEREAL_NVP(variables), CEREAL_NVP(bindings));
	}
	catch (...)
	{
		DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Save Failed EditorConfig[%s]", file_path.data());
		return;
	}
	DEBUG_LOG_CATEGORY(core::logger::categorys::system, "Save Succeeded EditorConfig[%s]", file_path.data());
}

void editor_config_data::apply_default_bindings()
{
	auto& manager = console_manager::instance();
	manager.clear_bindings();
	manager.bind("at", "console.toggle");
	manager.bind("f8", "editor.camera.toggle");
}
#endif	//	defined(USE_IMGUI)
