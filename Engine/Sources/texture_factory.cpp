#include "texture_factory.h"

#include <filesystem>
#include <string>
#include <vector>

#include "render_data.h"
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

	return std::make_shared<texture2d>(texture, shader_resource_view);
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

	return std::make_shared<texture2d>(texture, shader_resource_view);
}
