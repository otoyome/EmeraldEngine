#pragma once

#include <memory>
#include <string_view>

#include "Material/material_resource.h"

//	マテリアルリソースの作成
class material_resource_factory
{
public:
	material_resource_factory() = delete;

public:
	//	新規作成用の既定のマテリアルリソース
	static material_resource_ptr create_new(std::string_view name, material_resource::domains domain);

	//	参照先のマテリアルリソースが無い時の代わりのマテリアル
	static material_resource_ptr create_missing(const resource::asset_reference<material_resource>& reference, material_resource::domains domain);
};
