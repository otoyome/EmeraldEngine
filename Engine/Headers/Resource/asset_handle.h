#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "Resource/asset_reference.h"
#include "Resource/asset_registry.h"

namespace resource
{
	//	アセットへの参照と、読み込み済みのリソースをまとめて持つ
	template<class Type>
	class asset_handle
	{
	public:
		asset_handle() = default;
		asset_handle(std::nullptr_t) {}
		asset_handle(std::shared_ptr<Type> value) { set(std::move(value)); }

		asset_handle& operator=(std::shared_ptr<Type> value) { set(std::move(value)); return *this; }
		asset_handle& operator=(std::nullptr_t) { set(nullptr); return *this; }

		void set(std::shared_ptr<Type> value)
		{
			resource = std::move(value);
			reference = asset_reference<Type>::from(resource);
		}
		void set_reference(asset_reference<Type> value)
		{
			resource.reset();
			reference = std::move(value);
		}

		void set_legacy_path(std::string path)
		{
			const universally_unique_identifier uuid = ::asset_registry::is_valid() && !path.empty() ? ::asset_registry::instance()->find_uuid(path) : universally_unique_identifier{};
			set_reference({ uuid, std::move(path) });
		}

		asset_reference<Type> get_reference() const
		{
			return resource ? asset_reference<Type>::from(resource) : reference;
		}

		const std::shared_ptr<Type>& get() const { return resource; }
		operator const std::shared_ptr<Type>&() const { return resource; }
		Type* operator->() const { return resource.get(); }
		Type& operator*() const { return *resource; }
		explicit operator bool() const { return resource != nullptr; }
		bool is_empty() const { return !resource && reference.is_empty(); }

		template<class Archive>
		void save(Archive& archive) const
		{
			archive(get_reference());
		}

		template<class Archive>
		void load(Archive& archive)
		{
			asset_reference<Type> value;
			archive(value);
			set_reference(std::move(value));
		}

	private:
		asset_reference<Type> reference;
		std::shared_ptr<Type> resource;
	};
}	//	namespace resource
