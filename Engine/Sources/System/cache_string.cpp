#include "system/cache_string.h"

#include "system/system_utility.h"

utf8_cache::type utf8_cache::convert(std::string_view value)
{
	return system_utility::convert_shift_jis_to_utf8(value);
}

utf16_cache::type utf16_cache::convert(std::string_view value)
{
	return system_utility::convert_shift_jis_to_utf16(value);
}

lower_cache::type lower_cache::convert(std::string_view value)
{
	return system_utility::to_lower(value);
}

upper_cache::type upper_cache::convert(std::string_view value)
{
	return system_utility::to_upper(value);
}

extension_cache::type extension_cache::convert(std::string_view value)
{
	// std::filesystem::path::extension と同じく「.」「..」と先頭の「.」は拡張子にしない
	const std::string_view name = value.substr(value.rfind('/') + 1);
	if (name == "." || name == "..")
		return {};

	const std::size_t dot = name.rfind('.');
	if (dot == std::string_view::npos || dot == 0)
		return {};

	return lower_cache::convert(name.substr(dot));
}

cache_string::cache_string(const char* value)
{
	assign(value);
}

cache_string::cache_string(std::string value)
{
	assign(std::move(value));
}

cache_string::cache_string(std::string_view value)
{
	assign(value);
}

cache_string::cache_string(std::wstring_view value)
{
	assign(value);
}

cache_string::cache_string(const cache_string& other)
: value(other.value)
, cache_hash(other.cache_hash)
{
}

cache_string::cache_string(cache_string&& other) noexcept
: value(std::move(other.value))
, cache_hash(other.cache_hash)
, caches(std::move(other.caches))
{
	other.reset_moved();
}

void cache_string::clear()
{
	if (value.empty())
		return;

	value.clear();
	update_hash();

	invalidate();
}

void cache_string::assign(std::string value)
{
	if (this->value == value)
		return;

	this->value = std::move(value);
	update_hash();

	invalidate();
}

void cache_string::assign(std::string_view value)
{
	if (this->value == value)
		return;

	this->value.assign(value);
	update_hash();

	invalidate();
}

void cache_string::assign(const char* value)
{
	assert(value != nullptr);

	if (this->value == value)
		return;

	this->value = value;
	update_hash();

	invalidate();
}

void cache_string::assign(std::wstring_view value)
{
	const std::string shift_jis = system_utility::convert_utf16_to_shift_jis(value);
	assign(std::string_view(shift_jis));
}

const std::string& cache_string::str() const
{
	return value;
}

std::string_view cache_string::view() const
{
	return value;
}

const char* cache_string::c_str() const
{
	return value.c_str();
}

std::size_t cache_string::get_hash() const
{
	return cache_hash;
}

void cache_string::invalidate()
{
	caches.clear();
}

void cache_string::update_hash()
{
	cache_hash = value.empty() ? 0 : static_cast<std::size_t>(system_utility::generate_hash(value));
}

void cache_string::reset_moved()
{
	value.clear();
	cache_hash = 0;
	caches.clear();
}

bool cache_string::empty() const
{
	return value.empty();
}

cache_string::size_type cache_string::size() const
{
	return value.size();
}

void cache_string::reserve(size_type size)
{
	value.reserve(size);
}

cache_string::size_type cache_string::capacity() const
{
	return value.capacity();
}

cache_string::const_iterator cache_string::begin() const
{
	return value.begin();
}

cache_string::const_iterator cache_string::end() const
{
	return value.end();
}

cache_string::operator const std::string&() const
{
	return value;
}

cache_string::operator std::string_view() const
{
	return value;
}

cache_string& cache_string::operator=(const char* value)
{
	assign(value);
	return *this;
}

cache_string& cache_string::operator=(std::string value)
{
	assign(std::move(value));
	return *this;
}

cache_string& cache_string::operator=(std::string_view value)
{
	assign(value);
	return *this;
}

cache_string& cache_string::operator=(std::wstring_view value)
{
	assign(value);
	return *this;
}

cache_string& cache_string::operator=(const cache_string& other)
{
	if (this != &other)
	{
		assign(other.value);
	}
	return *this;
}

cache_string& cache_string::operator=(cache_string&& other) noexcept
{
	if (this != &other)
	{
		value = std::move(other.value);
		cache_hash = other.cache_hash;
		caches = std::move(other.caches);
		other.reset_moved();
	}
	return *this;
}

cache_string& cache_string::operator+=(const char* value)
{
	assert(value != nullptr);

	this->value += value;
	update_hash();

	invalidate();

	return *this;
}

cache_string& cache_string::operator+=(std::string_view value)
{
	this->value += value;
	update_hash();

	invalidate();

	return *this;
}

cache_string& cache_string::operator+=(char value)
{
	this->value += value;
	update_hash();

	invalidate();

	return *this;
}

char cache_string::operator[](size_type index) const
{
	return value[index];
}