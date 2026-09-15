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

cache_string::cache_string(const cache_string& other)
: value(other.value)
{
}

cache_string::cache_string(cache_string&& other) noexcept
: value(std::move(other.value))
, caches(std::move(other.caches))
{
}

void cache_string::clear()
{
	if (value.empty())
		return;

	value.clear();
	cache_hash = 0;

	invalidate();
}

void cache_string::assign(std::string value)
{
	if (this->value == value)
		return;

	this->value = std::move(value);
	cache_hash = static_cast<std::size_t>(system_utility::generate_hash(value));

	invalidate();
}

void cache_string::assign(std::string_view value)
{
	if (this->value == value)
		return;

	this->value.assign(value);
	cache_hash = static_cast<std::size_t>(system_utility::generate_hash(value));

	invalidate();
}

void cache_string::assign(const char* value)
{
	assert(value != nullptr);

	if (this->value == value)
		return;

	this->value = value;
	cache_hash = static_cast<std::size_t>(system_utility::generate_hash(value));

	invalidate();
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
		caches = std::move(other.caches);
	}
	return *this;
}

cache_string& cache_string::operator+=(const char* value)
{
	assert(value != nullptr);

	this->value += value;

	invalidate();

	return *this;
}

cache_string& cache_string::operator+=(std::string_view value)
{
	this->value += value;

	invalidate();

	return *this;
}

cache_string& cache_string::operator+=(char value)
{
	this->value += value;

	invalidate();

	return *this;
}

char cache_string::operator[](size_type index) const
{
	return value[index];
}