#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include <guiddef.h>

//	UUID クラス
class universally_unique_identifier
{
public:
	constexpr universally_unique_identifier() = default;
	explicit universally_unique_identifier(const GUID& guid);

	//	新しい UUID を作る
	static universally_unique_identifier generate();

	//	"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" 形式から作る
	static bool from_string(std::string_view text, universally_unique_identifier& out_value);

	//	"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" 形式で取り出す
	std::string to_string() const;

	GUID to_guid() const;

	//	上位/下位64bit取得
	constexpr std::uint64_t get_high() const { return high; }
	constexpr std::uint64_t get_low() const { return low; }

	//	空判定
	constexpr bool is_empty() const { return high == 0 && low == 0; }
	void clear() { high = 0; low = 0; }

	constexpr bool operator==(const universally_unique_identifier& other) const = default;
	constexpr auto operator<=>(const universally_unique_identifier& other) const = default;

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(high, low);
	}

private:
	std::uint64_t high = 0;
	std::uint64_t low = 0;
};

//	空の UUID
inline constexpr universally_unique_identifier invalid_universally_unique_identifier{};

template<>
struct std::hash<universally_unique_identifier>
{
	size_t operator()(const universally_unique_identifier& value) const noexcept
	{
		const size_t high_hash = std::hash<std::uint64_t>()(value.get_high());
		return high_hash ^ (std::hash<std::uint64_t>()(value.get_low()) + 0x9e3779b97f4a7c15ull + (high_hash << 6) + (high_hash >> 2));
	}
};
