#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cassert>
#include <utility>

// SJIS Å® UTF-8
struct utf8_cache
{
	using type = std::string;

	static type convert(std::string_view value);
};

// SJIS Å® UTF-16
struct utf16_cache
{
	using type = std::wstring;

	static type convert(std::string_view value);
};

// âpéöÇè¨ï∂éö
struct lower_cache
{
	using type = std::string;

	static type convert(std::string_view value);
};

// âpéöÇëÂï∂éö
struct upper_cache
{
	using type = std::string;

	static type convert(std::string_view value);
};

// Åu/ÅvãÊêÿÇËÇÃÉpÉXÇÃägí£éq
struct extension_cache
{
	using type = std::string;

	static type convert(std::string_view value);
};

class cache_string
{
	struct cache_data_base
	{
		virtual ~cache_data_base() = default;
	};

	template<class T>
	struct cache_data : cache_data_base
	{
		T value;
	};

	using cache_map = std::unordered_map<std::type_index, std::unique_ptr<cache_data_base>>;

public:
	using size_type      = std::string::size_type;
	using const_iterator = std::string::const_iterator;

public:
	cache_string() = default;
	cache_string(const char* value);
	cache_string(std::string value);
	cache_string(std::string_view value);
	cache_string(std::wstring_view value);
	cache_string(const cache_string& other);
	cache_string(cache_string&& other) noexcept;

	~cache_string() = default;

public:
	void clear();

	void assign(std::string value);
	void assign(std::string_view value);
	void assign(const char* value);
	void assign(std::wstring_view value);

	const std::string& str() const;
	std::string_view view() const;
	const char* c_str() const;
	std::size_t get_hash() const;

	bool empty() const;
	size_type size() const;
	void reserve(size_type size);
	size_type capacity() const;

public:
	const_iterator begin() const;
	const_iterator end() const;

public:
	operator const std::string&() const;
	operator std::string_view() const;

	cache_string& operator=(const char* value);
	cache_string& operator=(std::string value);
	cache_string& operator=(std::string_view value);
	cache_string& operator=(std::wstring_view value);
	cache_string& operator=(const cache_string& other);
	cache_string& operator=(cache_string&& other) noexcept;

	cache_string& operator+=(const char* value);
	cache_string& operator+=(std::string_view value);
	cache_string& operator+=(char value);

	char operator[](size_type index) const;

	friend bool operator==(const cache_string& lhs, const cache_string& rhs)
	{
		return lhs.value == rhs.value;
	}

	friend bool operator!=(const cache_string& lhs, const cache_string& rhs)
	{
		return !(lhs == rhs);
	}

public:
	template<class Cache>
	const typename Cache::type& get() const
	{
		using type = typename Cache::type;
		const std::type_index type_index(typeid(Cache));
		auto iterator = caches.find(type_index);
		if (iterator != caches.end())
			return static_cast<const cache_data<type>*>(iterator->second.get())->value;

		auto cache = std::make_unique<cache_data<type>>();
		cache->value = Cache::convert(value);

		const type& result = cache->value;
		caches.emplace(type_index, std::move(cache));
		return result;
	}

public:
	template<class Archive>
	std::string save_minimal(Archive const&) const
	{
		return value;
	}

	template<class Archive>
	void load_minimal(Archive const&, const std::string& value)
	{
		assign(value);
	}

private:
	void invalidate();
	void update_hash();
	void reset_moved();

private:
	std::string value;
	std::size_t cache_hash = 0;

	mutable cache_map caches;
};