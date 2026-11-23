#include "Core/cache_string.h"


core::utf8_cache::type core::utf8_cache::convert(std::string_view value)
{
	return core::convert_shift_jis_to_utf8(value);
}

core::utf16_cache::type core::utf16_cache::convert(std::string_view value)
{
	return core::convert_shift_jis_to_utf16(value);
}

core::lower_cache::type core::lower_cache::convert(std::string_view value)
{
	return core::to_lower(value);
}

core::upper_cache::type core::upper_cache::convert(std::string_view value)
{
	return core::to_upper(value);
}

core::extension_cache::type core::extension_cache::convert(std::string_view value)
{
	// std::filesystem::path::extension と同じく「.」「..」と先頭の「.」は拡張子にしない
	const std::string_view name = value.substr(value.rfind('/') + 1);
	if (name == "." || name == "..")
		return {};

	const std::size_t dot = name.rfind('.');
	if (dot == std::string_view::npos || dot == 0)
		return {};

	return core::lower_cache::convert(name.substr(dot));
}

core::cache_string::cache_string(const char* value)
{
	assign(value);
}

core::cache_string::cache_string(std::string value)
{
	assign(std::move(value));
}

core::cache_string::cache_string(std::string_view value)
{
	assign(value);
}

core::cache_string::cache_string(std::wstring_view value)
{
	assign(value);
}

core::cache_string::cache_string(const core::cache_string& other)
: value(other.value)
, cache_hash(other.cache_hash)
{
}

core::cache_string::cache_string(core::cache_string&& other) noexcept
: value(std::move(other.value))
, cache_hash(other.cache_hash)
, caches(std::move(other.caches))
{
	other.reset_moved();
}

void core::cache_string::clear()
{
	if (value.empty())
		return;

	value.clear();
	update_hash();

	invalidate();
}

void core::cache_string::assign(std::string value)
{
	if (this->value == value)
		return;

	this->value = std::move(value);
	update_hash();

	invalidate();
}

void core::cache_string::assign(std::string_view value)
{
	if (this->value == value)
		return;

	this->value.assign(value);
	update_hash();

	invalidate();
}

void core::cache_string::assign(const char* value)
{
	assert(value != nullptr);

	if (this->value == value)
		return;

	this->value = value;
	update_hash();

	invalidate();
}

void core::cache_string::assign(std::wstring_view value)
{
	const std::string shift_jis = core::convert_utf16_to_shift_jis(value);
	assign(std::string_view(shift_jis));
}

const std::string& core::cache_string::str() const
{
	return value;
}

std::string_view core::cache_string::view() const
{
	return value;
}

const char* core::cache_string::c_str() const
{
	return value.c_str();
}

std::size_t core::cache_string::get_hash() const
{
	return cache_hash;
}

void core::cache_string::invalidate()
{
	caches.clear();
}

void core::cache_string::update_hash()
{
	cache_hash = value.empty() ? 0 : static_cast<std::size_t>(core::generate_hash(value));
}

void core::cache_string::reset_moved()
{
	value.clear();
	cache_hash = 0;
	caches.clear();
}

bool core::cache_string::empty() const
{
	return value.empty();
}

core::cache_string::size_type core::cache_string::size() const
{
	return value.size();
}

void core::cache_string::reserve(size_type size)
{
	value.reserve(size);
}

core::cache_string::size_type core::cache_string::capacity() const
{
	return value.capacity();
}

core::cache_string::const_iterator core::cache_string::begin() const
{
	return value.begin();
}

core::cache_string::const_iterator core::cache_string::end() const
{
	return value.end();
}

core::cache_string::operator const std::string&() const
{
	return value;
}

core::cache_string::operator std::string_view() const
{
	return value;
}

core::cache_string& core::cache_string::operator=(const char* value)
{
	assign(value);
	return *this;
}

core::cache_string& core::cache_string::operator=(std::string value)
{
	assign(std::move(value));
	return *this;
}

core::cache_string& core::cache_string::operator=(std::string_view value)
{
	assign(value);
	return *this;
}

core::cache_string& core::cache_string::operator=(std::wstring_view value)
{
	assign(value);
	return *this;
}

core::cache_string& core::cache_string::operator=(const core::cache_string& other)
{
	if (this != &other)
	{
		assign(other.value);
	}
	return *this;
}

core::cache_string& core::cache_string::operator=(core::cache_string&& other) noexcept
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

core::cache_string& core::cache_string::operator+=(const char* value)
{
	assert(value != nullptr);

	this->value += value;
	update_hash();

	invalidate();

	return *this;
}

core::cache_string& core::cache_string::operator+=(std::string_view value)
{
	this->value += value;
	update_hash();

	invalidate();

	return *this;
}

core::cache_string& core::cache_string::operator+=(char value)
{
	this->value += value;
	update_hash();

	invalidate();

	return *this;
}

char core::cache_string::operator[](size_type index) const
{
	return value[index];
}