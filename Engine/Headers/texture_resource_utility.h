#pragma once

#include <string_view>
#include <vector>

#include "texture.h"
#include "texture_import_settings.h"
#include "Resource/asset_file_header.h"

namespace DirectX
{
	class ScratchImage;
}

//	テクスチャリソースのファイルを扱う処理
namespace texture_resource_utility
{
	//	テクスチャリソースのファイルからヘッダー・取り込み設定・DDS 本体を読む
	bool read_file(std::string_view file_path, resource::asset_file_header& out_header, texture_import_settings& out_settings, std::vector<char>& out_dds);

	//	DirectX::ScratchImageをのみを読み込む
	bool read_image(std::string_view file_path, DirectX::ScratchImage& out_image);

	//	テクスチャリソースを同期で読み込んでテクスチャを返す
	texture_ptr load_texture(std::string_view file_path);
}	//	namespace texture_resource_utility
