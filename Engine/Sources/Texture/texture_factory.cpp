#include "Texture/texture_factory.h"

#include <filesystem>
#include <string>
#include <vector>

#include "Graphics/Core/render_data.h"
#include "Texture/texture_resource.h"
#include "Texture/texture_tracker.h"
#include "DirectXTex.h"

bool texture_factory::decode_file(std::string_view file_path, DirectX::ScratchImage& out_image)
{
	const std::wstring name = std::filesystem::path(std::string(file_path)).wstring();
	DirectX::TexMetadata meta_data;

	if (SUCCEEDED(DirectX::GetMetadataFromDDSFile(name.c_str(), DirectX::DDS_FLAGS_NONE, meta_data)))
		return SUCCEEDED(DirectX::LoadFromDDSFile(name.c_str(), DirectX::DDS_FLAGS_NONE, &meta_data, out_image));

	if (SUCCEEDED(DirectX::GetMetadataFromHDRFile(name.c_str(), meta_data)))
		return SUCCEEDED(DirectX::LoadFromHDRFile(name.c_str(), &meta_data, out_image));

	if (SUCCEEDED(DirectX::GetMetadataFromTGAFile(name.c_str(), DirectX::TGA_FLAGS_NONE, meta_data)))
		return SUCCEEDED(DirectX::LoadFromTGAFile(name.c_str(), DirectX::TGA_FLAGS_NONE, &meta_data, out_image));

	if (SUCCEEDED(DirectX::GetMetadataFromWICFile(name.c_str(), DirectX::WIC_FLAGS_NONE, meta_data)))
		return SUCCEEDED(DirectX::LoadFromWICFile(name.c_str(), DirectX::WIC_FLAGS_NONE, &meta_data, out_image));

	return false;
}

bool texture_factory::decode_memory(const void* data, std::size_t size, DirectX::ScratchImage& out_image)
{
	const auto* bytes = static_cast<const std::uint8_t*>(data);
	DirectX::TexMetadata meta_data;

	if (SUCCEEDED(DirectX::GetMetadataFromDDSMemory(bytes, size, DirectX::DDS_FLAGS_NONE, meta_data)))
		return SUCCEEDED(DirectX::LoadFromDDSMemory(bytes, size, DirectX::DDS_FLAGS_NONE, &meta_data, out_image));

	if (SUCCEEDED(DirectX::GetMetadataFromHDRMemory(bytes, size, meta_data)))
		return SUCCEEDED(DirectX::LoadFromHDRMemory(bytes, size, &meta_data, out_image));

	if (SUCCEEDED(DirectX::GetMetadataFromTGAMemory(bytes, size, DirectX::TGA_FLAGS_NONE, meta_data)))
		return SUCCEEDED(DirectX::LoadFromTGAMemory(bytes, size, DirectX::TGA_FLAGS_NONE, &meta_data, out_image));

	if (SUCCEEDED(DirectX::GetMetadataFromWICMemory(bytes, size, DirectX::WIC_FLAGS_NONE, meta_data)))
		return SUCCEEDED(DirectX::LoadFromWICMemory(bytes, size, DirectX::WIC_FLAGS_NONE, &meta_data, out_image));

	return false;
}

texture2d_ptr texture_factory::create_from_image(const DirectX::ScratchImage& image)
{
	const auto& device = render_data::instance()->get_device();

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	HRESULT hr = DirectX::CreateShaderResourceView(device.Get(), image.GetImages(), image.GetImageCount(), image.GetMetadata(), shader_resource_view.GetAddressOf());
	if (FAILED(hr))
		return nullptr;

	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	shader_resource_view->GetResource(resource.GetAddressOf());

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	if (FAILED(resource.As(&texture)))
		return nullptr;

	auto created = std::make_shared<texture2d>(texture, shader_resource_view);
#if defined(USE_IMGUI)
	track(created);
#endif	//	defined(USE_IMGUI)
	return created;
}

texture2d_ptr texture_factory::create_solid_color(UINT width, UINT height, std::uint32_t color)
{
	const auto& device = render_data::instance()->get_device();

	std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * height, color);

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA subresource{};
	subresource.pSysMem = pixels.data();
	subresource.SysMemPitch = width * sizeof(std::uint32_t);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	if (FAILED(device->CreateTexture2D(&desc, &subresource, texture.GetAddressOf())))
		return nullptr;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, shader_resource_view.GetAddressOf())))
		return nullptr;

	auto created = std::make_shared<texture2d>(texture, shader_resource_view);
#if defined(USE_IMGUI)
	created->set_debug_name("solid_color");
	track(created);
#endif	//	defined(USE_IMGUI)
	return created;
}

bool texture_factory::create_texture2d(texture_ptr& out_texture, std::string_view debug_name, D3D11_TEXTURE2D_DESC desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	return create_texture2d_core(out_texture, debug_name, desc, nullptr, srv_desc, uav_desc);
}

bool texture_factory::create_texture2d(texture_ptr& out_texture, std::string_view debug_name, D3D11_TEXTURE2D_DESC desc, DWORD init_value/*0xAABBGGRR*/, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	std::vector<DWORD> sysmem(static_cast<std::size_t>(desc.Width) * desc.Height, init_value);

	D3D11_SUBRESOURCE_DATA subresource_data{};
	subresource_data.pSysMem = sysmem.data();
	subresource_data.SysMemPitch = sizeof(DWORD) * desc.Width;
	subresource_data.SysMemSlicePitch = 0;
	return create_texture2d_core(out_texture, debug_name, desc, &subresource_data, srv_desc, uav_desc);
}

bool texture_factory::create_texture2d(texture_ptr& out_texture, std::string_view debug_name, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA subresource_data, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	return create_texture2d_core(out_texture, debug_name, desc, &subresource_data, srv_desc, uav_desc);
}

bool texture_factory::create_texture2d_core(texture_ptr& out_texture, std::string_view debug_name, const D3D11_TEXTURE2D_DESC& desc, const D3D11_SUBRESOURCE_DATA* subresource_data, const D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, const D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	out_texture = nullptr;
	const auto& device = render_data::instance()->get_device();

	//	テクスチャ
	Microsoft::WRL::ComPtr<ID3D11Texture2D> resource;
	HRESULT hr = device->CreateTexture2D(&desc, subresource_data, resource.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	シェーダーリソースビュー生成
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	if (core::is_bitflag(desc.BindFlags, D3D11_BIND_SHADER_RESOURCE))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc{};
		if (srv_desc)
		{
			srvdesc = *srv_desc;
		}
		else
		{
			srvdesc.Format = desc.Format;
			srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvdesc.Texture2D.MipLevels = 1;
		}
		hr = device->CreateShaderResourceView(resource.Get(), &srvdesc, shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return false;
	}

	//	アンオーダードアクセスビュー生成
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view;
	if (core::is_bitflag(desc.BindFlags, D3D11_BIND_UNORDERED_ACCESS))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc{};
		if (uav_desc)
		{
			uavdesc = *uav_desc;
		}
		else
		{
			uavdesc.Format = desc.Format;
			uavdesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			uavdesc.Texture2D.MipSlice = 0;
		}
		hr = device->CreateUnorderedAccessView(resource.Get(), &uavdesc, unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return false;
	}

	out_texture = std::make_shared<texture2d>(resource, shader_resource_view, unordered_access_view);
#if defined(USE_IMGUI)
	out_texture->set_debug_name(debug_name);
	track(out_texture);
#endif	//	defined(USE_IMGUI)
	return true;
}

#if defined(USE_IMGUI)
void texture_factory::track(const texture_ptr& value)
{
	if (texture_tracker::is_valid())
		texture_tracker::instance()->add(value);
}
#endif	//	defined(USE_IMGUI)

std::shared_ptr<texture_resource> texture_factory::create_runtime_resource(texture_ptr value)
{
	auto created = std::make_shared<texture_resource>();
	created->value = std::move(value);
	created->set_missing(false);
	return created;
}
