#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <memory>

#include "texture.h"

class texture_resource;

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

	//	ランタイム用テクスチャ生成
	static std::shared_ptr<texture_resource> create_runtime_resource(texture_ptr value);

	//	描画先などの2Dテクスチャを作る(debug_name はプロファイラーの一覧に出す名前)
	static bool create_texture2d(texture_ptr& out_texture, std::string_view debug_name, D3D11_TEXTURE2D_DESC desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr);
	static bool create_texture2d(texture_ptr& out_texture, std::string_view debug_name, D3D11_TEXTURE2D_DESC desc, DWORD init_value, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr);
	static bool create_texture2d(texture_ptr& out_texture, std::string_view debug_name, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA subresource_data, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr);

private:
	static bool create_texture2d_core(texture_ptr& out_texture, std::string_view debug_name, const D3D11_TEXTURE2D_DESC& desc, const D3D11_SUBRESOURCE_DATA* subresource_data, const D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, const D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc);

#if defined(USE_IMGUI)
	static void track(const texture_ptr& value);
#endif	//	defined(USE_IMGUI)
};
