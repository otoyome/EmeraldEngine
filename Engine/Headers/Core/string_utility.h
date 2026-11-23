#pragma once

#include <string>
#include <string_view>

namespace core
{
	//	std::string用の終端文字でフィットさせる関数
	void fit_string_to_null_terminated(std::string& str);

	//	文字列変換
	std::string	replace(std::string_view text, std::string_view target_text, std::string_view replace_text);

	//	秒数を "分:秒:1/100秒" 形式の表示用文字列にする
	std::string to_display_time_string(float time);

}	//	namespace core
