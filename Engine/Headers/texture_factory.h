#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "texture.h"

namespace DirectX
{
	class ScratchImage;
}

//	GPU テクスチャの作成
class texture_factory
{
public:
	texture_factory() = delete;

public:
	//	テクスチャをデコード
	static bool decode_file(std::string_view file_path, DirectX::ScratchImage& out_image);

	//	メモリ上のテクスチャをデコードする
	static bool decode_memory(const void* data, std::size_t size, DirectX::ScratchImage& out_image);

	//	デコード済みのテクスチャから2Dテクスチャを作る
	static texture2d_ptr create_from_image(const DirectX::ScratchImage& image);

	//	1色で塗った2Dテクスチャを作る
	static texture2d_ptr create_solid_color(UINT width, UINT height, std::uint32_t color);
};
