#pragma once

#include <string>
#include <string_view>

namespace core
{
	//	文字コード変換器設定/破棄
	void initialize_charset_converter();
	void uninitialize_charset_converter();

	//	文字コード判定
	bool is_valid_utf16(std::wstring_view text);
	bool is_valid_utf8(std::string_view text);
	bool is_valid_sjis(std::string_view text);

	//	文字コード変換系
	std::string convert_shift_jis_to_utf8(std::string_view shift_jis);
	std::wstring convert_shift_jis_to_utf16(std::string_view shift_jis);

	std::string convert_utf8_to_shift_jis(std::string_view utf8);
	std::wstring convert_utf8_to_utf16(std::string_view utf8);

	std::string convert_utf16_to_shift_jis(std::wstring_view utf16);
	std::string convert_utf16_to_utf8(std::wstring_view utf16);

	//	文字コード自動変換系
	std::wstring convert_utf16(std::string_view text);
	std::wstring convert_utf16(std::wstring_view text);
	std::string convert_shift_jis(std::string_view text);
	std::string convert_shift_jis(std::wstring_view text);
	std::string convert_utf8(std::string_view text);
	std::string convert_utf8(std::wstring_view text);

	//	大文字・小文字変換(Shift-JIS)
	std::string to_lower(std::string_view shift_jis);
	std::string to_upper(std::string_view shift_jis);

	//	大文字・小文字変換(UTF-16)
	std::wstring to_lower(std::wstring_view utf16);
	std::wstring to_upper(std::wstring_view utf16);

}	//	namespace core
