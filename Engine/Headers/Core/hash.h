#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace core
{
	//	ハッシュ算出
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
	inline hash_key generate_hash(const void* data, std::size_t size)
	{
		return	fnv1a_hash(size, static_cast<const char*>(data));
	}

	//	ハッシュ合成
	template<class T>
	constexpr T hash_combine(T seed, T value)
	{
		static_assert(std::is_unsigned_v<T> && (sizeof(T) == 4 || sizeof(T) == 8), "hash_combine : 32bit/64bit の符号なし整数のみ対応");
		constexpr T golden_ratio = (sizeof(T) == 8) ? static_cast<T>(0x9e3779b97f4a7c15ull) : static_cast<T>(0x9e3779b9u);
		return	seed ^ (value + golden_ratio + (seed << 6) + (seed >> 2));
	}

}	//	namespace core
