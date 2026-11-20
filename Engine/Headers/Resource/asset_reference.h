#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <string_view>

#include "System/universally_unique_identifier.h"

namespace resource
{

	//	UUID とパスを持つアセットのリソース型
	template<class Type>
	concept asset_resource = requires(const Type& value)
	{
		{ value.get_uuid() } -> std::convertible_to<const universally_unique_identifier&>;
		{ value.get_file_path() } -> std::convertible_to<std::string_view>;
	};

	template<class Type>
	struct asset_reference
	{
		universally_unique_identifier uuid;

		bool is_empty() const { return uuid.is_empty(); }

		static asset_reference from(const std::shared_ptr<Type>& value)
		{
			static_assert(asset_resource<Type>);
			if (!value)
				return {};
			return { value->get_uuid() };
		}

		template<class Archive>
		void save(Archive& archive) const
		{
			archive(uuid);
		}

		template<class Archive>
		void load(Archive& archive)
		{
			archive(uuid);
		}
	};
}	//	namespace resource
