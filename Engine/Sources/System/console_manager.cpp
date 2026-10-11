#include "System/console_manager.h"

#include <cctype>
#include <charconv>
#include <fstream>

namespace
{
	//	出力として残す最大行数
	static constexpr size_t max_output_lines = 1024;

	std::string to_lower(std::string_view text)
	{
		std::string result(text);
		for (auto& c : result)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return result;
	}

	std::string_view trim(std::string_view text)
	{
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
			text.remove_prefix(1);
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
			text.remove_suffix(1);
		return text;
	}

	//	「"」の外にある「;」で区切る
	std::vector<std::string_view> split_statements(std::string_view line)
	{
		std::vector<std::string_view> result;
		bool in_quote = false;
		size_t begin = 0;
		for (size_t index = 0; index < line.size(); ++index)
		{
			if (line[index] == '"')
				in_quote = !in_quote;
			else if (line[index] == ';' && !in_quote)
			{
				result.emplace_back(line.substr(begin, index - begin));
				begin = index + 1;
			}
		}
		result.emplace_back(line.substr(begin));
		return result;
	}

	std::string describe(const console_object& object)
	{
		std::string text = object.get_name();
		if (object.is_variable())
			text += " = " + static_cast<const console_variable_base&>(object).to_string();
		if (!object.get_help().empty())
			text += "    // " + object.get_help();
		return text;
	}

#if	defined(USE_IMGUI)
	//	組み込みコマンド
	console_command help_command("help", "コンソール要素のリストを表示する（help フィルター）",
		[](const console_command::arguments& args)
		{
			auto& manager = console_manager::instance();
			for (auto* object : manager.collect(""))
			{
				if (!args.empty() && object->get_name().find(args[0]) == std::string::npos)
					continue;
				manager.print(describe(*object));
			}
		});

	console_command list_command("list", "前方一致する要素を表示する（list 接頭辞）",
		[](const console_command::arguments& args)
		{
			auto& manager = console_manager::instance();
			for (auto* object : manager.collect(args.empty() ? std::string_view() : std::string_view(args[0])))
				manager.print(describe(*object));
		});

	console_command toggle_command("toggle", "bool 変数を反転する（toggle 名前）",
		[](const console_command::arguments& args)
		{
			auto& manager = console_manager::instance();
			if (args.empty())
			{
				manager.print("usage: toggle 名前");
				return;
			}
			auto variable = dynamic_cast<console_variable<bool>*>(manager.find_variable(args[0]));
			if (!variable)
			{
				manager.print("bool 変数ではありません: " + args[0]);
				return;
			}
			variable->set(!variable->get());
			manager.print(describe(*variable));
		});

	console_command reset_command("reset", "変数を既定値に戻す（reset 名前）",
		[](const console_command::arguments& args)
		{
			auto& manager = console_manager::instance();
			if (args.empty())
			{
				manager.print("usage: reset 名前");
				return;
			}
			auto variable = manager.find_variable(args[0]);
			if (!variable)
			{
				manager.print("変数が見つかりません: " + args[0]);
				return;
			}
			variable->reset();
			manager.print(describe(*variable));
		});

	console_command exec_command("exec", "ファイルに書かれたコマンドを順に実行する（exec ファイル）",
		[](const console_command::arguments& args)
		{
			auto& manager = console_manager::instance();
			if (args.empty())
			{
				manager.print("usage: exec ファイル");
				return;
			}
			std::ifstream stream(args[0]);
			if (!stream.is_open())
			{
				manager.print("ファイルを開けません: " + args[0]);
				return;
			}
			std::string line;
			while (std::getline(stream, line))
			{
				std::string_view text = trim(line);
				if (text.empty() || text.front() == '#' || text.starts_with("//"))
					continue;
				manager.execute(text);
			}
		});

	console_command bind_command("bind", "キーにコマンドを割り当てる（bind キー コマンド）",
		[](const console_command::arguments& args)
		{
			auto& manager = console_manager::instance();
			if (args.size() < 2)
			{
				manager.print("usage: bind キー コマンド");
				return;
			}
			std::string command;
			for (size_t index = 1; index < args.size(); ++index)
			{
				if (index > 1)
					command += ' ';
				command += args[index];
			}
			if (!manager.bind(args[0], command))
				manager.print("不明なキーです: " + args[0]);
		});

	console_command unbind_command("unbind", "キーの割り当てを解除する（unbind キー）",
		[](const console_command::arguments& args)
		{
			auto& manager = console_manager::instance();
			if (args.empty() || !manager.unbind(args[0]))
				manager.print("usage: unbind 割り当て済みのキー");
		});

	console_command bindings_command("bindings", "キー割り当てのリストを表示する",
		[](const console_command::arguments&)
		{
			auto& manager = console_manager::instance();
			for (auto& [key, command] : manager.get_bindings())
				manager.print(key + " : " + command);
		});

	console_command clear_command("clear", "コンソールの出力を消去する",
		[](const console_command::arguments&)
		{
			console_manager::instance().clear_output();
		});
#endif	//	defined(USE_IMGUI)
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace console_detail
{
	std::string to_string(bool value) { return value ? "1" : "0"; }
	std::string to_string(int value) { return std::to_string(value); }
	std::string to_string(float value)
	{
		char buffer[64];
		auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
		return std::string(buffer, result.ptr);
	}
	std::string to_string(const std::string& value) { return value; }

	bool from_string(std::string_view text, bool& value)
	{
		const std::string lower = to_lower(trim(text));
		if (lower == "1" || lower == "true" || lower == "on")
		{
			value = true;
			return true;
		}
		if (lower == "0" || lower == "false" || lower == "off")
		{
			value = false;
			return true;
		}
		return false;
	}
	bool from_string(std::string_view text, int& value)
	{
		text = trim(text);
		auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		return result.ec == std::errc() && result.ptr == text.data() + text.size();
	}
	bool from_string(std::string_view text, float& value)
	{
		text = trim(text);
		auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		return result.ec == std::errc() && result.ptr == text.data() + text.size();
	}
	bool from_string(std::string_view text, std::string& value)
	{
		value = std::string(text);
		return true;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

console_object::console_object(std::string_view name, std::string_view help, uint32_t flags)
: name(name)
, help(help)
, flags(flags)
{
	console_manager::instance().register_object(this);
}

console_object::~console_object()
{
	console_manager::instance().unregister_object(this);
}

console_command::console_command(std::string_view name, std::string_view help, function func, uint32_t flags)
: console_object(name, help, flags)
, func(std::move(func))
{
}

void console_command::execute(const arguments& args) const
{
	if (func)
		func(args);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

console_manager& console_manager::instance()
{
	static console_manager manager;
	return manager;
}

console_manager::console_manager()
{
}

console_manager::~console_manager()
{
}

void console_manager::register_object(console_object* object)
{
	_ASSERT_EXPR(objects.find(object->get_name()) == objects.end(), L"console object name is duplicated");
	objects.insert_or_assign(object->get_name(), object);
}

void console_manager::unregister_object(console_object* object)
{
	auto it = objects.find(object->get_name());
	if (it != objects.end() && it->second == object)
		objects.erase(it);
}

console_object* console_manager::find(std::string_view name) const
{
	auto it = objects.find(name);
	return it != objects.end() ? it->second : nullptr;
}

console_variable_base* console_manager::find_variable(std::string_view name) const
{
	auto object = find(name);
	return object && object->is_variable() ? static_cast<console_variable_base*>(object) : nullptr;
}

std::vector<const console_object*> console_manager::collect(std::string_view prefix) const
{
	std::vector<const console_object*> result;
	for (auto it = objects.lower_bound(prefix); it != objects.end(); ++it)
	{
		if (!it->first.starts_with(prefix))
			break;
		result.emplace_back(it->second);
	}
	return result;
}

std::vector<std::string> console_manager::tokenize(std::string_view line)
{
	std::vector<std::string> tokens;
	std::string current;
	bool in_quote = false;
	bool has_token = false;
	for (char c : line)
	{
		if (c == '"')
		{
			in_quote = !in_quote;
			has_token = true;
			continue;
		}
		if (!in_quote && std::isspace(static_cast<unsigned char>(c)))
		{
			if (has_token)
			{
				tokens.emplace_back(std::move(current));
				current.clear();
				has_token = false;
			}
			continue;
		}
		current += c;
		has_token = true;
	}
	if (has_token)
		tokens.emplace_back(std::move(current));
	return tokens;
}

void console_manager::execute(std::string_view line)
{
	for (auto statement : split_statements(line))
	{
		statement = trim(statement);
		if (!statement.empty())
			execute_statement(statement);
	}
}

void console_manager::execute_statement(std::string_view statement)
{
	print("> " + std::string(statement));

	auto tokens = tokenize(statement);
	if (tokens.empty())
		return;

	auto object = find(tokens[0]);
	if (!object)
	{
		print("不明なコマンドです: " + tokens[0]);
		return;
	}

	console_command::arguments args(tokens.begin() + 1, tokens.end());
	if (!object->is_variable())
	{
		static_cast<console_command*>(object)->execute(args);
		return;
	}

	auto variable = static_cast<console_variable_base*>(object);
	if (args.empty())
	{
		print(describe(*variable));
		return;
	}
	if (variable->is_flag(console_flags::read_only))
	{
		print("読み取り専用です: " + variable->get_name());
		return;
	}
	if (!variable->from_string(args[0]))
	{
		print("値を解釈できません: " + args[0]);
		return;
	}
	print(describe(*variable));
}

#if	defined(USE_IMGUI)
bool console_manager::bind(std::string_view key, std::string_view command)
{
	const std::string key_name = to_lower(key);
	if (!magic_enum::enum_cast<keyboard::key_states>(key_name).has_value())
		return false;
	bindings.insert_or_assign(key_name, std::string(command));
	return true;
}

bool console_manager::unbind(std::string_view key)
{
	return bindings.erase(to_lower(key)) > 0;
}

void console_manager::clear_bindings()
{
	bindings.clear();
}

void console_manager::update_bindings()
{
	if (!keyboard::is_valid())
		return;

	const bool text_input = ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;

	const auto& state = keyboard::instance()->get_state();
	std::vector<std::string> commands;
	for (auto& [key, command] : bindings)
	{
		auto key_state = magic_enum::enum_cast<keyboard::key_states>(key);
		if (!key_state.has_value())
			continue;
		const bool function_key = *key_state >= keyboard::key_states::f1 && *key_state <= keyboard::key_states::f12;
		const bool system_key = function_key || *key_state == keyboard::key_states::at;
		if (text_input && !system_key)
			continue;
		if (state.is_key_state(*key_state) == input::state::down)
			commands.emplace_back(command);
	}

	//	実行中に割り当てが変わってもよいように、集めてから実行する
	for (auto& command : commands)
		execute(command);
}
#endif	//	defined(USE_IMGUI)

void console_manager::print(std::string_view text)
{
	output.emplace_back(text);
	if (output.size() > max_output_lines)
		output.erase(output.begin(), output.begin() + (output.size() - max_output_lines));
}
