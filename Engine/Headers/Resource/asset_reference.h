#pragma once

#include <memory>
#include <string>

#include "System/universally_unique_identifier.h"

namespace resource
{
	//	UUID によるアセットの参照
	template<class Type>
	struct asset_reference
	{
		universally_unique_identifier uuid;
		std::string path;

		bool is_empty() const { return uuid.is_empty() && path.empty(); }

		//	読み込み済みのリソースから作る
		static asset_reference from(const std::shared_ptr<Type>& value)
		{
			if (!value)
				return {};
			return { value->get_uuid(), std::string(value->get_file_path()) };
		}

		template<class Archive>
		void serialize(Archive& archive)
		{
			archive(uuid, path);
		}
	};
}	//	namespace resource
