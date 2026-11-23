#pragma once

#include <string>
#include <string_view>

namespace core
{
	//	相対・絶対パス変換系
	std::string convert_to_absolute_path(std::string_view relative_path);
	std::string convert_to_relative_path(std::string_view absolute_path);

	//	「/」に変換
	std::string	preferred_path(std::string_view path);

}	//	namespace core
