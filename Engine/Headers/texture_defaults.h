#pragma once

#include "texture.h"

//	既定のテクスチャ
class texture_defaults
{
	SINGLETON(texture_defaults)

public:
	texture_defaults();
	~texture_defaults();

	const texture_ptr& get_white() const { return white; }
	const texture_ptr& get_black() const { return black; }
	const texture_ptr& get_loading() const { return loading; }
	const texture_ptr& get_missing() const { return missing; }

private:
	texture_ptr white;
	texture_ptr black;
	texture_ptr missing;
	texture_ptr loading;
};
