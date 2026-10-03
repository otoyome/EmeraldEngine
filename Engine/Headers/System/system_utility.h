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

	DirectX::XMFLOAT3 convert_xmfloat3_from_vector(DirectX::XMVECTOR value);
	DirectX::XMFLOAT4 convert_xmfloat4_from_vector(DirectX::XMVECTOR value);
	DirectX::XMFLOAT4X4 convert_xmfloat4x4_from_matrix(DirectX::XMMATRIX value);

	std::string to_display_time_string(float time);

	inline math::vector2f to_math(const DirectX::XMFLOAT2& v) { return math::vector2f(v.x, v.y); }
	inline math::vector3f to_math(const DirectX::XMFLOAT3& v) { return math::vector3f(v.x, v.y, v.z); }
	inline math::vector4f to_math(const DirectX::XMFLOAT4& v) { return math::vector4f(v.x, v.y, v.z, v.w); }
	inline math::quaternionf to_math_quaternion(const DirectX::XMFLOAT4& q) { return math::quaternionf(q.x, q.y, q.z, q.w); }
	inline math::quaternionf to_math_quaternion(DirectX::FXMVECTOR q) { DirectX::XMFLOAT4 v; DirectX::XMStoreFloat4(&v, q); return to_math_quaternion(v); }
	inline math::rotatorf to_math_rotator(const DirectX::XMFLOAT3& euler) { return math::rotatorf(euler.x, euler.y, euler.z); }
	inline math::vector3f to_math_vector3(DirectX::FXMVECTOR v) { DirectX::XMFLOAT3 f; DirectX::XMStoreFloat3(&f, v); return to_math(f); }
	inline math::matrixf to_math(const DirectX::XMFLOAT4X4& m)
	{
		math::matrixf ret;
		ret.set_storage(m._11, m._12, m._13, m._14, m._21, m._22, m._23, m._24, m._31, m._32, m._33, m._34, m._41, m._42, m._43, m._44);
		return ret;
	}

	inline DirectX::XMFLOAT2 to_xmfloat(const math::vector2f& v) { return DirectX::XMFLOAT2(v.x, v.y); }
	inline DirectX::XMFLOAT3 to_xmfloat(const math::vector3f& v) { return DirectX::XMFLOAT3(v.x, v.y, v.z); }
	inline DirectX::XMFLOAT4 to_xmfloat(const math::vector4f& v) { return DirectX::XMFLOAT4(v.x, v.y, v.z, v.w); }
	inline DirectX::XMFLOAT4 to_xmfloat(const math::quaternionf& q) { return DirectX::XMFLOAT4(q.x, q.y, q.z, q.w); }
	inline DirectX::XMFLOAT3 to_xmfloat(const math::rotatorf& r) { return DirectX::XMFLOAT3(r.x, r.y, r.z); }
	inline DirectX::XMFLOAT4X4 to_xmfloat(const math::matrixf& m)
	{
		return DirectX::XMFLOAT4X4(m.s11, m.s12, m.s13, m.s14, m.s21, m.s22, m.s23, m.s24, m.s31, m.s32, m.s33, m.s34, m.s41, m.s42, m.s43, m.s44);
	}
	inline DirectX::XMVECTOR to_xmvector(const math::quaternionf& q) { return DirectX::XMVectorSet(q.x, q.y, q.z, q.w); }
}	//	namespace system_utility

// シリアライズ
namespace DirectX
{
	template<class Archive>
	void serialize(Archive& archive, XMINT2& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMINT3& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMINT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMUINT2& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMUINT3& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMUINT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT2& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT3& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4X4& m)
	{
		archive(
			cereal::make_nvp("_11", m._11), cereal::make_nvp("_12", m._12), cereal::make_nvp("_13", m._13), cereal::make_nvp("_14", m._14),
			cereal::make_nvp("_21", m._21), cereal::make_nvp("_22", m._22), cereal::make_nvp("_23", m._23), cereal::make_nvp("_24", m._24),
			cereal::make_nvp("_31", m._31), cereal::make_nvp("_32", m._32), cereal::make_nvp("_33", m._33), cereal::make_nvp("_34", m._34),
			cereal::make_nvp("_41", m._41), cereal::make_nvp("_42", m._42), cereal::make_nvp("_43", m._43), cereal::make_nvp("_44", m._44)
		);
	}
}

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
