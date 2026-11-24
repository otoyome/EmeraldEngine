#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include <guiddef.h>

#include "Core/hash.h"

namespace core
{
	//	UUID クラス
	class uuid
	{
	public:
		constexpr uuid() = default;
		explicit uuid(const GUID& guid);

		//	新しい UUID を作る
		static uuid generate();

		//	"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" 形式から作る
		static bool from_string(std::string_view text, uuid& out_value);

		//	"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" 形式で取り出す
		std::string to_string() const;

		GUID to_guid() const;

		//	上位/下位64bit取得
		constexpr std::uint64_t get_high() const { return high; }
		constexpr std::uint64_t get_low() const { return low; }

		//	空判定
		constexpr bool is_empty() const { return high == 0 && low == 0; }
		void clear() { high = 0; low = 0; }

		constexpr bool operator==(const uuid& other) const = default;
		constexpr auto operator<=>(const uuid& other) const = default;

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
	inline constexpr uuid invalid_uuid{};

}	//	namespace core

template<>
struct std::hash<core::uuid>
{
	size_t operator()(const core::uuid& value) const noexcept
	{
		const size_t high_hash = std::hash<std::uint64_t>()(value.get_high());
		return core::hash_combine<size_t>(high_hash, std::hash<std::uint64_t>()(value.get_low()));
	}
};
