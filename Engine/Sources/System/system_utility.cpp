#include "System/system_utility.h"

#include <codecvt>
#include	<regex>

//  ICUライブラリー関係
#include "unicode/ucsdet.h"
#include "unicode/putil.h"
#include "unicode/ucnv.h"
#include "unicode/udata.h"
#include "unicode/unistr.h"

namespace
{
	enum class charset
	{
		ansii,
		shift_jis,
		utf_8,
		utf_16,
		utf_16le,
		utf_16be,

		max,
	};
	constexpr const char* charset_texts[] =
	{
		"windows-1252",	//	<=ここはロケール依存になる
		"Shift-JIS",
		"UTF-8",
		"UTF-16",
		"UTF-16LE",
		"UTF-16BE",
	};

	//  Shift-JIS用にdatを読込む
	struct icu_sjis_dat_setter
	{
		std::vector<char> sjis_buffer;

		icu_sjis_dat_setter(std::string_view dat_path)
		{
			FILE* fp{ nullptr };
			fopen_s(&fp, dat_path.data(), "rb");
			if (fp)
			{
				fseek(fp, 0, SEEK_END);
				long size{ ftell(fp) };
				fseek(fp, 0, SEEK_SET);

				sjis_buffer.resize(size);
				fread(sjis_buffer.data(), size, 1, fp);
				fclose(fp);

				UErrorCode status = U_ZERO_ERROR;
				udata_setCommonData(sjis_buffer.data(), &status);
				if (U_FAILURE(status))
				{
					DEBUG_ERROR_CATEGORY(logger::categorys::system, "charset converter Shift-JIS.dat not found");
				}
			}
		}
	};
	std::unique_ptr<icu_sjis_dat_setter> sjis_dat_setter;

	//  文字コード変換器コンテナ
	class icu_converter_container
	{
	public:
		~icu_converter_container()
		{
			for (auto& converter : converters)
			{
				if (converter)
					ucnv_close(converter);
			}
		}

		UConverter* get(charset set)
		{
			if (!converters[static_cast<int>(set)])
			{
				UErrorCode status = U_ZERO_ERROR;
				converters[static_cast<int>(set)] = ucnv_open(charset_texts[static_cast<int>(set)], &status);
				if (U_FAILURE(status))
				{
					//DEBUG_ERROR_CATEGORY(logger::categorys::system, "charset converter error");
					return nullptr;
				}
			}
			//	内部情報をクリアしてから渡す
			ucnv_reset(converters[static_cast<int>(set)]);
			return converters[static_cast<int>(set)];
		}

		void work_uchar_buffer_resize(int32_t size)
		{
			if (size > work_uchar_buffer.size())
				work_uchar_buffer.resize(size);
			work_uchar_buffer[size - 1] = '\0';
		}

		void work_string_resize(int32_t size)
		{
			if (size > work_string.size())
				work_string.resize(size);
			work_string[size - 1] = '\0';
		}

	public:	//	文字列変換判定
		bool is_valid_charcode_detected(const char* text_code, size_t length, charset set)
		{
			//  厳密に判定するため往復変換判定
			UConverter* conv = get(set);
			if (!conv)
				return false;

			UErrorCode status = U_ZERO_ERROR;
			int32_t u16_len = ucnv_toUChars(conv, nullptr, 0, text_code, (int32_t)length, &status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return false;

			status = U_ZERO_ERROR;
			work_uchar_buffer_resize(u16_len);
			ucnv_toUChars(conv, work_uchar_buffer.data(), u16_len, text_code, (int32_t)length, &status);
			if (U_FAILURE(status))
				return false;

			status = U_ZERO_ERROR;
			int32_t back_len = ucnv_fromUChars(conv, nullptr, 0, work_uchar_buffer.data(), u16_len, &status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return false;

			status = U_ZERO_ERROR;
			std::string back(back_len, '\0');
			ucnv_fromUChars(conv, back.data(), back_len, work_uchar_buffer.data(), u16_len, &status);
			if (U_FAILURE(status))
				return false;

			return back == std::string(text_code, length);
		}

		//  未検証
		bool is_valid_charcode_detected(const wchar_t* text_code, size_t length, charset set)
		{
			UConverter* conv = get(set);
			if (!conv)
				return false;

			const UChar* src = reinterpret_cast<const UChar*>(text_code);
			int32_t u16_len = static_cast<int32_t>(length);

			UErrorCode status = U_ZERO_ERROR;
			int32_t encoded_len = ucnv_fromUChars(conv, nullptr, 0, src, u16_len, &status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return false;

			status = U_ZERO_ERROR;
			std::string encoded(encoded_len + 1, '\0');
			ucnv_fromUChars(conv, encoded.data(), encoded_len + 1, src, u16_len, &status);
			if (U_FAILURE(status))
				return false;

			status = U_ZERO_ERROR;
			int32_t back_len = ucnv_toUChars(conv, nullptr, 0, encoded.data(), encoded_len + 1, &status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return false;

			status = U_ZERO_ERROR;
			if (back_len)
				work_uchar_buffer_resize(back_len + 1);
			ucnv_toUChars(conv, work_uchar_buffer.data(), back_len + 1, encoded.data(), encoded_len + 1, &status);
			if (U_FAILURE(status))
				return false;

			return std::memcmp(work_uchar_buffer.data(),
							   src,
							   sizeof(UChar) * u16_len) == 0;
		}

		template<typename ... Codes>
		bool is_valid_charcode_detected(const char* text_code, size_t length, Codes... char_codes)
		{
			return (is_valid_charcode_detected(text_code, length, char_codes) || ...);
		}

		template<typename ... Codes>
		bool is_valid_charcode_detected(const wchar_t* text_code, size_t length, Codes... char_codes)
		{
			return (is_valid_charcode_detected(text_code, length, char_codes) || ...);
		}

		template<typename ... Codes>
		bool is_valid_charcodes(const std::string_view text, Codes... char_codes)
		{
			return text.empty() ? true : is_valid_charcode_detected(text.data(), text.size() + 1, char_codes...);
		}

		template<typename ... Codes>
		bool is_valid_charcodes(const std::wstring_view text, Codes... char_codes)
		{
			return text.empty() ? true : is_valid_charcode_detected(text.data(), text.length() + 1, char_codes...);
		}

	public:	//	文字列変換関数群
		std::string convert_shift_jis_to_utf8(std::string_view sjis)
		{
			UConverter* sjis_converter = get(charset::shift_jis);
			UConverter* utf8_converter = get(charset::utf_8);
			if (!sjis_converter || !utf8_converter)
				return "";

			UErrorCode status = U_ZERO_ERROR;
			int32_t u16_len = ucnv_toUChars(sjis_converter, nullptr, 0,
				sjis.data(), static_cast<int32_t>(sjis.size()), &status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return "";

			status = U_ZERO_ERROR;
			work_uchar_buffer_resize(u16_len + 1);
			ucnv_toUChars(sjis_converter,
				work_uchar_buffer.data(), static_cast<int32_t>(work_uchar_buffer.size()),
				sjis.data(), static_cast<int32_t>(sjis.size()),
				&status);
			if (U_FAILURE(status))
				return "";

			status = U_ZERO_ERROR;
			int32_t utf8_len = ucnv_fromUChars(utf8_converter, nullptr, 0,
				work_uchar_buffer.data(), u16_len, &status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return "";

			status = U_ZERO_ERROR;
			work_string_resize(utf8_len + 1);
			ucnv_fromUChars(utf8_converter,
				work_string.data(), static_cast<int32_t>(work_string.size()),
				work_uchar_buffer.data(), u16_len, &status);

			if (U_FAILURE(status))
				return "";

			return work_string.substr(0, utf8_len);
		}

		std::wstring convert_shift_jis_to_utf16(std::string_view sjis)
		{
			UConverter* sjis_converter = get(charset::shift_jis);
			if (!sjis_converter)
				return std::wstring();

			UErrorCode status = U_ZERO_ERROR;
			int32_t u16_len = ucnv_toUChars(sjis_converter, nullptr, 0,
				sjis.data(), static_cast<int32_t>(sjis.size()), &status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return std::wstring();

			status = U_ZERO_ERROR;
			work_uchar_buffer_resize(u16_len + 1);
			ucnv_toUChars(sjis_converter,
				work_uchar_buffer.data(), static_cast<int32_t>(work_uchar_buffer.size()),
				sjis.data(), static_cast<int32_t>(sjis.size()), &status);
			if (U_FAILURE(status))
				return std::wstring();

			return std::wstring(reinterpret_cast<wchar_t*>(work_uchar_buffer.data()), u16_len);
		}

		std::string convert_utf8_to_shift_jis(std::string_view utf8)
		{
			UConverter* sjis_converter = get(charset::shift_jis);
			UConverter* utf8_converter = get(charset::utf_8);
			if (!sjis_converter || !utf8_converter)
				return "";

			UErrorCode status = U_ZERO_ERROR;
			int32_t u16_len = ucnv_toUChars(utf8_converter, nullptr, 0,
				utf8.data(), static_cast<int32_t>(utf8.size()),
				&status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return "";

			status = U_ZERO_ERROR;
			work_uchar_buffer_resize(u16_len + 1);
			ucnv_toUChars(utf8_converter,
				work_uchar_buffer.data(), static_cast<int32_t>(work_uchar_buffer.size()),
				utf8.data(), static_cast<int32_t>(utf8.size()),
				&status);
			if (U_FAILURE(status))
				return "";

			status = U_ZERO_ERROR;
			int32_t sjis_len = ucnv_fromUChars(sjis_converter,
				nullptr, 0,
				work_uchar_buffer.data(), u16_len,
				&status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return "";

			status = U_ZERO_ERROR;
			work_string_resize(sjis_len + 1);
			ucnv_fromUChars(sjis_converter,
				work_string.data(), static_cast<int32_t>(work_string.size()),
				work_uchar_buffer.data(), u16_len,
				&status);

			if (U_FAILURE(status))
				return "";

			return work_string.substr(0, sjis_len);
		}

		std::wstring convert_utf8_to_utf16(std::string_view utf8)
		{
			UConverter* utf8_converter = get(charset::utf_8);
			if (!utf8_converter)
				return std::wstring();

			UErrorCode status = U_ZERO_ERROR;
			int32_t u16_len = ucnv_toUChars(utf8_converter,
				nullptr, 0,
				utf8.data(), static_cast<int32_t>(utf8.size()),
				&status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return std::wstring();

			status = U_ZERO_ERROR;
			work_uchar_buffer_resize(u16_len + 1);
			ucnv_toUChars(utf8_converter,
				work_uchar_buffer.data(), static_cast<int32_t>(work_uchar_buffer.size()),
				utf8.data(), static_cast<int32_t>(utf8.size()),
				&status);
			if (U_FAILURE(status))
				return std::wstring();

			return std::wstring(reinterpret_cast<wchar_t*>(work_uchar_buffer.data()), u16_len);
		}

		std::string convert_utf16_to_shift_jis(std::wstring_view utf16)
		{
			UConverter* sjis_converter = get(charset::shift_jis);
			if (!sjis_converter)
				return "";

			const UChar* u16 = reinterpret_cast<const UChar*>(utf16.data());
			int32_t u16_len = static_cast<int32_t>(utf16.size());

			UErrorCode status = U_ZERO_ERROR;
			int32_t sjis_len = ucnv_fromUChars(sjis_converter,
				nullptr, 0,
				u16, u16_len,
				&status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return "";

			status = U_ZERO_ERROR;
			work_string_resize(sjis_len + 1);
			ucnv_fromUChars(sjis_converter, work_string.data(),
							static_cast<int32_t>(work_string.size()), u16, u16_len, &status);

			if (U_FAILURE(status))
				return "";

			return work_string.substr(0, sjis_len);
		}

		std::string convert_utf16_to_utf8(std::wstring_view utf16)
		{
			UConverter* utf8_converter = get(charset::utf_8);
			if (!utf8_converter)
				return "";

			const UChar* u16 = reinterpret_cast<const UChar*>(utf16.data());
			int32_t u16_len = static_cast<int32_t>(utf16.size());

			UErrorCode status = U_ZERO_ERROR;
			int32_t utf8_len = ucnv_fromUChars(utf8_converter, nullptr, 0, u16, u16_len, &status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				return "";

			status = U_ZERO_ERROR;
			work_string_resize(utf8_len + 1);
			ucnv_fromUChars(utf8_converter, work_string.data(), static_cast<int32_t>(work_string.size()),
				u16, u16_len, &status);

			if (U_FAILURE(status))
				return "";

			return work_string.substr(0, utf8_len);
		}

	private:
		UConverter* converters[static_cast<int>(charset::max)] = {};
		std::vector<UChar> work_uchar_buffer;
		std::string work_string;
	};
	thread_local std::unique_ptr<icu_converter_container> converter_container;
	icu_converter_container* get_converter_container()
	{
		if (!converter_container)
			converter_container = std::make_unique<icu_converter_container>();
		return converter_container.get();
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void system_utility::initialize_charset_converter()
{
	if (!sjis_dat_setter)
		sjis_dat_setter = std::make_unique<icu_sjis_dat_setter>("Assets/Resources/Systems/icudt79l.dat");
}

void system_utility::uninitialize_charset_converter()
{
	sjis_dat_setter.reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
bool system_utility::is_valid_utf16(std::wstring_view text)
{
	return get_converter_container()->is_valid_charcodes(text, charset::utf_16, charset::utf_16le, charset::utf_16be);
}

bool system_utility::is_valid_utf8(std::string_view text)
{
	return get_converter_container()->is_valid_charcodes(text, charset::utf_8);
}

bool system_utility::is_valid_sjis(std::string_view text)
{
	return get_converter_container()->is_valid_charcodes(text, charset::ansii, charset::shift_jis);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
std::string system_utility::convert_shift_jis_to_utf8(std::string_view sjis)
{
	return get_converter_container()->convert_shift_jis_to_utf8(sjis);
}

std::wstring system_utility::convert_shift_jis_to_utf16(std::string_view sjis)
{
	return get_converter_container()->convert_shift_jis_to_utf16(sjis);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
std::string system_utility::convert_utf8_to_shift_jis(std::string_view utf8)
{
	return get_converter_container()->convert_utf8_to_shift_jis(utf8);
}

std::wstring system_utility::convert_utf8_to_utf16(std::string_view utf8)
{
	return get_converter_container()->convert_utf8_to_utf16(utf8);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
std::string system_utility::convert_utf16_to_shift_jis(std::wstring_view utf16)
{
	return get_converter_container()->convert_utf16_to_shift_jis(utf16);
}

std::string system_utility::convert_utf16_to_utf8(std::wstring_view utf16)
{
	return get_converter_container()->convert_utf16_to_utf8(utf16);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
std::wstring system_utility::convert_utf16(std::string_view text)
{
	if (is_valid_utf8(text))
		return convert_utf8_to_utf16(text);
	if (is_valid_sjis(text))
		return convert_shift_jis_to_utf16(text);

	return std::wstring();
}

std::wstring system_utility::convert_utf16(std::wstring_view text)
{
	if (is_valid_utf16(text))
		return std::wstring(text);

	return std::wstring(text);
}

std::string system_utility::convert_shift_jis(std::string_view text)
{
	if (is_valid_utf8(text))
		return convert_utf8_to_shift_jis(text);

	return std::string(text);
}

std::string system_utility::convert_shift_jis(std::wstring_view text)
{
	if (is_valid_utf16(text))
		return convert_utf16_to_shift_jis(text);

	return std::string();
}

std::string system_utility::convert_utf8(std::string_view text)
{
	if (is_valid_utf8(text))
		return std::string(text);
	if (is_valid_sjis(text))
		return convert_shift_jis_to_utf8(text);

	return std::string(text);
}

std::string system_utility::convert_utf8(std::wstring_view text)
{
	if (is_valid_utf16(text))
		return convert_utf16_to_shift_jis(text);

	return std::string();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
std::string system_utility::convert_to_absolute_path(std::string_view relative_path)
{
	char absolute_path[MAX_PATH];
	if (_fullpath(absolute_path, relative_path.data(), MAX_PATH) != nullptr)
	{
		return absolute_path;
	}
	return "";
}

std::string system_utility::convert_to_relative_path(std::string_view absolute_path)
{
	std::string path = system_utility::replace(absolute_path, "\\\\", "/");

	char current_directry[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, current_directry);

	std::filesystem::path relative_path = std::filesystem::relative(path, current_directry);
	return system_utility::replace(relative_path.string(), "\\\\", "/");
}

void system_utility::fit_string_to_null_terminated(std::string& str)
{
	const auto pos = str.find('\0');
	if (pos != std::string::npos)
	{
		str.resize(pos);
	}
};

std::string	system_utility::replace(std::string_view text, std::string_view target_text, std::string_view replace_text)
{
	return	std::regex_replace(text.data(), std::regex(target_text.data()), replace_text.data());
}

std::string	system_utility::preferred_path(std::string_view path)
{
	return	system_utility::replace(path, "\\\\", "/");
}

const char* system_utility::str_case_str(const char* text, const char* pattern)
{
	if (!*pattern)
		return	text;

	for (; *text; ++text)
	{
		if (toupper(static_cast<unsigned char>(*text)) == toupper(static_cast<unsigned char>(*pattern)))
		{
			for (size_t index = 1;; ++index)
			{
				if (!pattern[index])
					return	text;

			if (toupper(static_cast<unsigned char>(text[index])) != toupper(static_cast<unsigned char>(pattern[index])))
				break;
			}
		}
	}
	return	nullptr;
}

std::string system_utility::to_display_time_string(float time)
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
