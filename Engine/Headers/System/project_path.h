#pragma	once

#include <filesystem>
#include <string>
#include <string_view>

//	プロジェクトパス
class project_path
{
public:
	static const std::filesystem::path& get_root();

public:
	project_path() = delete;

	//	起動時に呼んでルートを控える
	static void initialize();

	//	ルートからの相対パスにする
	static std::string to_relative(std::string_view path);

	//	ルートを基準に絶対パスにする
	static std::string to_absolute(std::string_view path);

	//	区切りを「/」に揃える
	static std::string normalize(std::string_view path);

private:
	static std::filesystem::path make_absolute(std::string_view path);

private:
	static std::filesystem::path root;
};
