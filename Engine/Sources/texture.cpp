#include "texture.h"

#include <algorithm>

namespace
{
	//	アドレスモード変換テーブル
	constexpr D3D11_TEXTURE_ADDRESS_MODE address_mode_table[] =
	{
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_TEXTURE_ADDRESS_MIRROR,
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_TEXTURE_ADDRESS_BORDER,
		D3D11_TEXTURE_ADDRESS_MIRROR_ONCE,
	};
	static_assert(ARRAYSIZE(address_mode_table) == static_cast<size_t>(texture_sampler_desc::address_modes::max));

	D3D11_TEXTURE_ADDRESS_MODE to_d3d11_address_mode(texture_sampler_desc::address_modes mode)
	{
		size_t index = static_cast<size_t>(mode);
		return index < ARRAYSIZE(address_mode_table) ? address_mode_table[index] : D3D11_TEXTURE_ADDRESS_WRAP;
	}
}

D3D11_SAMPLER_DESC texture_sampler_desc::to_d3d11_desc() const
{
	D3D11_SAMPLER_DESC desc{};
	switch (filter)
	{
		case filters::point:		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	break;
		case filters::anisotropic:	desc.Filter = D3D11_FILTER_ANISOTROPIC;			break;
		case filters::linear:
		default:					desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	break;
	}
	desc.AddressU = to_d3d11_address_mode(address_u);
	desc.AddressV = to_d3d11_address_mode(address_v);
	desc.AddressW = to_d3d11_address_mode(address_w);
	desc.MipLODBias = mip_lod_bias;
	desc.MaxAnisotropy = std::clamp<UINT>(max_anisotropy, 1, D3D11_REQ_MAXANISOTROPY);
	desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	for (size_t index = 0; index < border_color.size(); ++index)
	{
		desc.BorderColor[index] = border_color[index];
	}
	desc.MinLOD = min_lod;
	desc.MaxLOD = max_lod;
	return desc;
}

D3D11_RESOURCE_DIMENSION texture::get_dimension() const
{
	return dimension;
}

const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& texture::get_shader_resource_view() const
{
	return shader_resource_view;
}

const Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& texture::get_unordered_access_view() const
{
	return unordered_access_view;
}

texture::texture(D3D11_RESOURCE_DIMENSION dimension, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view)
: system_flags(0)
, dimension(dimension)
, shader_resource_view(shader_resource_view)
, unordered_access_view(unordered_access_view)
, sampler_desc()
{
	set_flag(flags::srgb, false);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
texture2d::texture2d(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view)
: texture(D3D11_RESOURCE_DIMENSION_TEXTURE2D, shader_resource_view, unordered_access_view)
{
	texture->GetDesc(&desc);

	//	初期設定
	set_flag(flags::cube_map, desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE);
}

texture2d::~texture2d()
{
}
