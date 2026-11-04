
#include "render_data.h"

#include <d3d11_1.h>

render_data::render_data(CONST HWND hwnd)
: hwnd(hwnd)
{
	HRESULT hr{ S_OK };

	// デバイス＆スワップチェーン生成
	{
		UINT create_device_flags{ 0 };
#ifdef _DEBUG
		create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_FEATURE_LEVEL feature_levels{ D3D_FEATURE_LEVEL_11_1 };

		DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
		swap_chain_desc.BufferCount = 1;
		swap_chain_desc.BufferDesc.Width = SCREEN_WIDTH;
		swap_chain_desc.BufferDesc.Height = SCREEN_HEIGHT;
		swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
		swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.OutputWindow = hwnd;
		swap_chain_desc.SampleDesc.Count = 1;
		swap_chain_desc.SampleDesc.Quality = 0;
		swap_chain_desc.Windowed = !FULLSCREEN;
		hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, create_device_flags,
			&feature_levels, 1, D3D11_SDK_VERSION, &swap_chain_desc,
			swap_chain.GetAddressOf(), device.GetAddressOf(), NULL, immediate_context.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	if (!swap_chain)
		return;

	state_cache.initialize(device.Get());

	//	VS から描き込み先の配列番号を出力できるかチェック
	{
		D3D11_FEATURE_DATA_D3D11_OPTIONS3 options{};
		hr = device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &options, sizeof(options));
		supported_device = SUCCEEDED(hr) && options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer;
	}

	// サンプラステートの生成
	{
		D3D11_SAMPLER_DESC sampler_desc{};
		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.MipLODBias = 0;
		sampler_desc.MaxAnisotropy = 16;
		sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		sampler_desc.BorderColor[0] = 0;
		sampler_desc.BorderColor[1] = 0;
		sampler_desc.BorderColor[2] = 0;
		sampler_desc.BorderColor[3] = 0;
		sampler_desc.MinLOD = 0;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
		sampler_states[static_cast<size_t>(SAMPLER_STATE::POINT)] = state_cache.get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_states[static_cast<size_t>(SAMPLER_STATE::LINEAR)] = state_cache.get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_states[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC)] = state_cache.get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_states[static_cast<size_t>(SAMPLER_STATE::POINT_CLAMP)] = state_cache.get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_states[static_cast<size_t>(SAMPLER_STATE::LINEAR_CLAMP)] = state_cache.get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_states[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC_CLAMP)] = state_cache.get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.BorderColor[0] = 0;
		sampler_desc.BorderColor[1] = 0;
		sampler_desc.BorderColor[2] = 0;
		sampler_desc.BorderColor[3] = 0;
		sampler_states[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_BLACK)] = state_cache.get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.BorderColor[0] = 1;
		sampler_desc.BorderColor[1] = 1;
		sampler_desc.BorderColor[2] = 1;
		sampler_desc.BorderColor[3] = 1;
		sampler_states[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_WHITE)] = state_cache.get_sampler_state(sampler_desc);
	}
}

render_data::~render_data()
{
}

void render_data::upload_gpu_data()
{
	upload_list.execute(immediate_context.Get());
}

void render_data::polling_gpu_readback()
{
	readback_queue.poll(immediate_context.Get());
}
