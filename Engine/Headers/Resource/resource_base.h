#pragma once

#include <string>
#include <string_view>

#include "System/universally_unique_identifier.h"

namespace resource
{
	//	リソース基底クラス
	class resource_base
	{
	public:
		resource_base() : resource_uuid(universally_unique_identifier::generate()) {}
		virtual ~resource_base() = default;

		resource_base(const resource_base& other) : file_path(other.file_path), resource_uuid(universally_unique_identifier::generate()) {}
		resource_base& operator=(const resource_base& other) { file_path = other.file_path; return *this; }

		void set_file_path(std::string_view path) { file_path = path; }
		std::string_view get_file_path() const { return file_path; }

		const universally_unique_identifier& get_uuid() const { return resource_uuid; }

		void inherit_uuid(const universally_unique_identifier& value) { resource_uuid = value; }

		void regenerate_uuid() { resource_uuid = universally_unique_identifier::generate(); }

		bool is_missing() const { return missing; }
		void set_missing(bool value) { missing = value; }

	private:
		std::string file_path;
		universally_unique_identifier resource_uuid;
		bool missing = false;
	};
}	//	namespace resource
