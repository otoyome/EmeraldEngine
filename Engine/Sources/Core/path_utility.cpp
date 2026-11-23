#include "Core/path_utility.h"

#include <filesystem>

std::string core::convert_to_absolute_path(std::string_view relative_path)
{
	char absolute_path[MAX_PATH];
	if (_fullpath(absolute_path, relative_path.data(), MAX_PATH) != nullptr)
	{
		return absolute_path;
	}
	return "";
}

std::string core::convert_to_relative_path(std::string_view absolute_path)
{
	std::string path = core::replace(absolute_path, "\\\\", "/");

	char current_directry[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, current_directry);

	std::filesystem::path relative_path = std::filesystem::relative(path, current_directry);
	return core::replace(relative_path.string(), "\\\\", "/");
}

std::string	core::preferred_path(std::string_view path)
{
	return	core::replace(path, "\\\\", "/");
}
