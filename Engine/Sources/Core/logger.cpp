#include "Core/logger.h"

#include <crtdbg.h>
#include <stdarg.h>
#include <stdio.h>

core::logger::logger()
: log_max(1024 * 2)
{
}

core::logger::~logger()
{
}

void core::logger::print(categorys category, const char* format, ...)
{
	char message[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	register_log(category, levels::normal, message);

	std::string m = message;
	m += "\n";
	OutputDebugStringA(m.c_str());
}

void core::logger::warning(categorys category, const char* format, ...)
{
	char message[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	register_log(category, levels::warning, message);

	std::string m = message;
	m += "\n";
	OutputDebugStringA(m.c_str());
}

void core::logger::error(categorys category, const char* format, ...)
{
	char message[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	register_log(category, levels::error, message);

	std::string m = message;
	m += "\n";
	OutputDebugStringA(m.c_str());
}

std::vector<const char*> core::logger::get_category_texts()
{
	std::vector<const char*> texts;
	for (auto category : magic_enum::enum_values<categorys>())
	{
		const char* text = magic_enum::enum_names<categorys>()[static_cast<int>(category)].data();
		if (categorys::max == category)
			text = "all";
		texts.push_back(text);
	}
	return texts;
}

std::vector<const char*> core::logger::get_levels_texts()
{
	std::vector<const char*> texts;
	for (auto level : magic_enum::enum_values<levels>())
	{
		const char* text = magic_enum::enum_names<levels>()[static_cast<int>(level)].data();
		if (levels::max == level)
			text = "all";
		texts.push_back(text);
	}
	return texts;
}

void core::logger::clear()
{
	std::lock_guard<std::mutex> lock(critical_section);
	logs.clear();
}

void core::logger::register_log(categorys category, levels level, const char* text)
{
	std::lock_guard<std::mutex> lock(critical_section);
	logs.emplace_back(category, level, text);
	while (logs.size() > log_max)
	{
		logs.pop_front();
	}
}
