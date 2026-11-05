#include "System/project_path.h"

std::filesystem::path project_path::root;

void project_path::initialize()
{
	std::error_code error;
	root = std::filesystem::current_path(error);
}

const std::filesystem::path& project_path::get_root()
{
	return root;
}

std::string project_path::to_relative(std::string_view path)
{
	std::error_code error;
	const std::filesystem::path relative = std::filesystem::relative(make_absolute(path), root, error);
	if (error || relative.empty())
		return normalize(path);
	return relative.generic_string();
}

std::string project_path::to_absolute(std::string_view path)
{
	return make_absolute(path).generic_string();
}

std::string project_path::normalize(std::string_view path)
{
	return std::filesystem::path(std::string(path)).generic_string();
}

std::filesystem::path project_path::make_absolute(std::string_view path)
{
	const std::filesystem::path source{ std::string(path) };
	return (source.is_absolute() ? source : root / source).lexically_normal();
}
