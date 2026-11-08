#include "graphics/hiz_buffer.h"

#include "Graphics/render_data.h"

namespace
{
	struct hiz_constants
	{
		math::uint2 dst_size;
		math::uint2 src_size;
	};

	std::shared_ptr<compute_shader> load_cs(std::string_view path)
	{
		load_shader_file desc;
		desc.file_path = path;
#if defined(_DEBUG)
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)
		std::shared_ptr<compute_shader> shader;
		shader_manager::instance()->load_shader<compute_shader>(shader, desc);
		return shader;
	}
}

hiz_buffer::hiz_buffer()
{
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(hiz_constants);
	HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&bd, nullptr, hiz_constant_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hiz_copy_cs = load_cs("Engine/EngineShader/Culling/hiz_copy_cs.hlsl");
	hiz_downsample_cs = load_cs("Engine/EngineShader/Culling/hiz_downsample_cs.hlsl");
}

hiz_buffer::~hiz_buffer()
{
}

void hiz_buffer::resize_buffer(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;
	if (width == last_width && height == last_height && hiz_texture)
		return;
	last_width = width;
	last_height = height;

	auto device = render_data::instance()->get_device();
	HRESULT hr;

	//	ミップ段数を算出(ミップ0は元解像度以下の2のべき乗にする)
	UINT hiz_width = 1, hiz_height = 1;
	while (hiz_width * 2 <= static_cast<UINT>(width))
		hiz_width *= 2;
	while (hiz_height * 2 <= static_cast<UINT>(height))
		hiz_height *= 2;

	hiz_mip_count = 1;
	{
		UINT m = (hiz_width > hiz_height) ? hiz_width : hiz_height;
		while (m > 1)
		{
			m >>= 1;
			++hiz_mip_count;
		}
	}
	//	16段階以上はいらない
	if (hiz_mip_count > 16)
		hiz_mip_count = 16;

	//	Hi-Z用バッファ生成
	D3D11_TEXTURE2D_DESC td{};
	td.Width = hiz_width;
	td.Height = hiz_height;
	td.MipLevels = hiz_mip_count;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R32_FLOAT;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	hr = device->CreateTexture2D(&td, nullptr, hiz_texture.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = DXGI_FORMAT_R32_FLOAT;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MostDetailedMip = 0;
		sd.Texture2D.MipLevels = hiz_mip_count;
		hr = device->CreateShaderResourceView(hiz_texture.Get(), &sd, hiz_srv.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	hiz_mip_srvs.assign(hiz_mip_count, nullptr);
	hiz_mip_uavs.assign(hiz_mip_count, nullptr);
	hiz_mip_sizes.assign(hiz_mip_count, {});
	for (UINT mip = 0; mip < hiz_mip_count; ++mip)
	{
		UINT mw = hiz_width >> mip;  if (mw == 0) mw = 1;
		UINT mh = hiz_height >> mip; if (mh == 0) mh = 1;
		hiz_mip_sizes[mip] = { mw, mh };

		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = DXGI_FORMAT_R32_FLOAT;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MostDetailedMip = mip;
		sd.Texture2D.MipLevels = 1;
		hr = device->CreateShaderResourceView(hiz_texture.Get(), &sd, hiz_mip_srvs[mip].ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};
		ud.Format = DXGI_FORMAT_R32_FLOAT;
		ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		ud.Texture2D.MipSlice = mip;
		hr = device->CreateUnorderedAccessView(hiz_texture.Get(), &ud, hiz_mip_uavs[mip].ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

void hiz_buffer::build(graphics_context& context)
{
	if (!hiz_texture || hiz_mip_count == 0)
		return;

	auto dc = context.device_context;

	//	ミップ0生成
	hiz_constants constant{};
	constant.dst_size = hiz_mip_sizes[0];
	constant.src_size = { static_cast<UINT>(last_width), static_cast<UINT>(last_height) };
	dc->UpdateSubresource(hiz_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
	dc->CSSetConstantBuffers(0, 1, hiz_constant_buffer.GetAddressOf());
	dc->CSSetShader(hiz_copy_cs->get_shader().Get(), nullptr, 0);
	dc->CSSetShaderResources(0, 1, context.scene_depth_texture->get_shader_resource_view().GetAddressOf());
	dc->CSSetUnorderedAccessViews(0, 1, hiz_mip_uavs[0].GetAddressOf(), nullptr);
	dc->Dispatch((hiz_mip_sizes[0].x + 7) / 8, (hiz_mip_sizes[0].y + 7) / 8, 1);
	context.clear_unordered_access_views();
	context.clear_shader_resorce_views();

	//	ミップ1以降生成
	dc->CSSetShader(hiz_downsample_cs->get_shader().Get(), nullptr, 0);
	for (UINT mip = 1; mip < hiz_mip_count; ++mip)
	{
		hiz_constants constant{};
		constant.dst_size = hiz_mip_sizes[mip];
		constant.src_size = hiz_mip_sizes[mip - 1];
		dc->UpdateSubresource(hiz_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
		dc->CSSetConstantBuffers(0, 1, hiz_constant_buffer.GetAddressOf());
		dc->CSSetShaderResources(0, 1, hiz_mip_srvs[mip - 1].GetAddressOf());
		dc->CSSetUnorderedAccessViews(0, 1, hiz_mip_uavs[mip].GetAddressOf(), nullptr);
		dc->Dispatch((hiz_mip_sizes[mip].x + 7) / 8, (hiz_mip_sizes[mip].y + 7) / 8, 1);
		context.clear_unordered_access_views();
		context.clear_shader_resorce_views();
	}

	dc->CSSetShader(nullptr, nullptr, 0);
}
