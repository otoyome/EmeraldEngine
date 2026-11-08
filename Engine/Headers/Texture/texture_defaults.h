#pragma once

#include "Texture/texture.h"
#include "Texture/texture_resource.h"

//	既定のテクスチャ
class texture_defaults
{
	SINGLETON(texture_defaults)

public:
	texture_defaults();
	~texture_defaults();

	const texture_ptr& get_white() const { return white->get_texture(); }
	const texture_ptr& get_black() const { return black->get_texture(); }
	const texture_ptr& get_loading() const { return loading->get_texture(); }
	const texture_ptr& get_missing() const { return missing->get_texture(); }

	const texture_resource_ptr& get_white_resource() const { return white; }
	const texture_resource_ptr& get_black_resource() const { return black; }

private:
	texture_resource_ptr white;
	texture_resource_ptr black;
	texture_resource_ptr missing;
	texture_resource_ptr loading;
};
