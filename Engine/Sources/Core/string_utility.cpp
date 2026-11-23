#include "Core/string_utility.h"

#include <regex>

void core::fit_string_to_null_terminated(std::string& str)
{
	const auto pos = str.find('\0');
	if (pos != std::string::npos)
	{
		str.resize(pos);
	}
}

std::string	core::replace(std::string_view text, std::string_view target_text, std::string_view replace_text)
{
	return	std::regex_replace(text.data(), std::regex(target_text.data()), replace_text.data());
}

std::string core::to_display_time_string(float time)
{
	int minits = static_cast<int>(time / 60);
	int second = static_cast<int>(fmodf(time, 60));
	int micro = static_cast<int>((time - minits * 60 - second) * 100);
	if (minits >= 100)
	{
		minits = 99;
		second = 59;
		micro = 99;
	}

	std::string t;
	if (minits < 10)
		t += "0";
	t += std::to_string(minits);
	t += ":";
	if (second < 10)
		t += "0";
	t += std::to_string(second);
	t += ":";
	if (micro < 10)
		t += "0";
	t += std::to_string(micro);
	return t;
}
