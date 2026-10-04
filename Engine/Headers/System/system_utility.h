#pragma	once

#define	NOMINMAX
#include <tchar.h>
#include <sstream>
#include <algorithm>
#include <string>

#include <string>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <filesystem>

#include <d3d11.h>
#include <wrl.h>
#include <cassert>

#include "logger.h"

#include "enum_class_attribute.h"
#include "system_units.h"

#include "cereal/cereal.hpp"
#include "magic_enum/include/magic_enum/magic_enum.hpp"
#include "system/cache_string.h"
#include "Math/math_all.h"

namespace	system_utility
{
	// ハッシュ算出
	using hash_key = unsigned long long;
	constexpr hash_key fnv_basis	= 14695981039346656037ull;
	constexpr hash_key fnv_prime	= 1099511628211ull;
	constexpr hash_key fnv_multiply	= 0x9ddfea08eb382d69ull;
	constexpr hash_key fnv1a_hash(hash_key numeric, const char* text, hash_key hash = fnv_basis )
	{
		return numeric > 0 ? fnv1a_hash(numeric - 1, text + 1, (hash ^ *text) * fnv_prime) : hash;
	}
	template<hash_key numeric>
	constexpr hash_key fnv1a_hash(const char (&array)[numeric])
	{
		return	fnv1a_hash(numeric - 1, &array[0]);
	}
	constexpr hash_key generate_hash(std::string_view text)
	{
		return	fnv1a_hash(text.length(), text.data());
	}

	template<typename	LeftType, typename	RightType>
	inline	bool	equals( const std::weak_ptr<LeftType>& left, const std::weak_ptr<RightType>& right )
	{
		return	!left.owner_before( right ) && !right.owner_before( left );
	}

	template<typename	LeftType, typename	RightType>
	inline	bool	equals( const std::weak_ptr<LeftType>& left, const std::shared_ptr<RightType>& right )
	{
		return !left.owner_before( left ) && !right.owner_before( left );
	}

	template<typename	Type>
	struct	is_iterable
	{
	private:
		template<typename	UnderlingType>
		static	constexpr	auto	check_adl( UnderlingType&& _rrValue )
		-> decltype( begin( _rrValue ), end( _rrValue ), std::true_type() );
		static	constexpr	std::false_type	check_adl( ... );

		template<typename	UnderlingType>
		static	constexpr	auto	check_std( UnderlingType&& _rrValue )
		-> decltype( std::begin( _rrValue ), std::end( _rrValue ), std::true_type() );
		static	constexpr	std::false_type	check_std( ... );

		template<typename	UnderlingType>
		static	constexpr	auto	check_member( UnderlingType&& _rrValue )
		-> decltype( _rrValue.begin(), _rrValue.end(), std::true_type() );
		static	constexpr	std::false_type	check_member(...);

	public:
		static	constexpr	bool	value	= ( decltype( check_adl( std::declval<Type>() ) )::value ||
												decltype( check_std( std::declval<Type>() ) )::value ||
												decltype( check_member( std::declval<Type>() ) )::value );
	};

	template<bool Test, typename Ty = void>
	using	enable_if_type	= typename std::enable_if<Test, Ty>::type;

	//	unique_ptr用キャスト処理
	template<typename Derived, typename Type, typename enable_if_type<!std::is_same<Derived, Type>{}, std::nullptr_t> = nullptr>
	inline	std::unique_ptr<Derived>	static_pointer_cast( const std::unique_ptr<Type>& pointer )
	{
		return	std::unique_ptr<Derived>( static_cast<Derived*>( pointer.release() ) );
	}

	//	unique_ptr用同じ型同士のキャスト
	template<typename Derived, typename Type, typename enable_if_type<std::is_same<Derived, Type>{}, std::nullptr_t> = nullptr>
	inline	std::unique_ptr<Derived>	static_pointer_cast( const std::unique_ptr<Type>& pointer )
	{
		return	std::unique_ptr<Derived>( pointer.release() );
	}

	template<typename BitFlagType, typename FlagType>
	bool is_bitflag(const BitFlagType& bit_flags, FlagType flag)
	{
		return bit_flags & static_cast<BitFlagType>(flag);
	}

	template<typename BitFlagType, typename FlagType>
	void set_bitflag(BitFlagType& bit_flags, FlagType flag, bool value)
	{
		if (value)	bit_flags |= static_cast<BitFlagType>(flag);
		else		bit_flags &= ~static_cast<BitFlagType>(flag);
	}

	//	即時クリアしたいとき用
	template<typename Type>
	void std_vector_swap_clear(std::vector<Type>& vector)
	{
		vector.clear();
		std::vector<Type> v;
		std::swap(vector, v);
	}

	//	std::string用の書式変換関数
	template<typename ... Args>
	std::string	format(std::string_view _rFormat, Args ... _pArgs)
	{
		size_t	sLength = std::snprintf(nullptr, 0, _rFormat.data(), _pArgs ...);
		std::vector<char> vBuf(sLength + 1);
		std::snprintf(&vBuf[0], sLength + 1, _rFormat.data(), _pArgs ...);
		return	std::string(&vBuf[0], &vBuf[0] + sLength);
	}

	//	std::string用の終端文字でフィットさせる関数
	void fit_string_to_null_terminated(std::string& str);

	//	文字列変換
	std::string	replace(std::string_view text, std::string_view target_text, std::string_view replace_text);

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

	//	相対・絶対パス変換系
	std::string convert_to_absolute_path(std::string_view relative_path);
	std::string convert_to_relative_path(std::string_view absolute_path);

	//	「/」に変換
	std::string	preferred_path(std::string_view path);

	const char* str_case_str(const char* text, const char* pattern);

	std::string to_display_time_string(float time);

}	//	namespace system_utility

namespace math
{
	template<class Archive, typename T>
	void serialize(Archive& archive, basic_element2<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, basic_element3<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, basic_element4<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, basic_element_matrix<T, 4, 4>& m)
	{
		auto& e = m.elements;
		archive(
			cereal::make_nvp("_11", e[0]), cereal::make_nvp("_12", e[1]), cereal::make_nvp("_13", e[2]), cereal::make_nvp("_14", e[3]),
			cereal::make_nvp("_21", e[4]), cereal::make_nvp("_22", e[5]), cereal::make_nvp("_23", e[6]), cereal::make_nvp("_24", e[7]),
			cereal::make_nvp("_31", e[8]), cereal::make_nvp("_32", e[9]), cereal::make_nvp("_33", e[10]), cereal::make_nvp("_34", e[11]),
			cereal::make_nvp("_41", e[12]), cereal::make_nvp("_42", e[13]), cereal::make_nvp("_43", e[14]), cereal::make_nvp("_44", e[15])
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, vector2<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, vector3<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, vector4<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, quaternion<T>& q)
	{
		archive(
			cereal::make_nvp("x", q.x),
			cereal::make_nvp("y", q.y),
			cereal::make_nvp("z", q.z),
			cereal::make_nvp("w", q.w)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, matrix<T>& m)
	{
		archive(
			cereal::make_nvp("_11", m.s11), cereal::make_nvp("_12", m.s12), cereal::make_nvp("_13", m.s13), cereal::make_nvp("_14", m.s14),
			cereal::make_nvp("_21", m.s21), cereal::make_nvp("_22", m.s22), cereal::make_nvp("_23", m.s23), cereal::make_nvp("_24", m.s24),
			cereal::make_nvp("_31", m.s31), cereal::make_nvp("_32", m.s32), cereal::make_nvp("_33", m.s33), cereal::make_nvp("_34", m.s34),
			cereal::make_nvp("_41", m.s41), cereal::make_nvp("_42", m.s42), cereal::make_nvp("_43", m.s43), cereal::make_nvp("_44", m.s44)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, rotatorf& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}
}
