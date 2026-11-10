
#include "Graphics/render_data.h"
#include "Graphics/render_pass.h"

#include <d3d11_1.h>
#include <dxgi1_5.h>

render_data::render_data(CONST HWND hwnd)
: hwnd(hwnd)
{
	HRESULT hr{ S_OK };

	// デバイス＆スワップチェーン生成
	{
		UINT create_device_flags{ 0 };
#ifdef	defined(_DEBUG)
		create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif	//	defined(_DEBUG)

		D3D_FEATURE_LEVEL feature_levels{ D3D_FEATURE_LEVEL_11_1 };

		hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, create_device_flags,
			&feature_levels, 1, D3D11_SDK_VERSION,
			device.GetAddressOf(), NULL, immediate_context.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return;

		//	デバイスを作ったアダプターのファクトリーからスワップチェーンを作る
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
		hr = device.As(&dxgi_device);
		if (SUCCEEDED(hr))
			hr = dxgi_device->GetAdapter(adapter.GetAddressOf());
		if (SUCCEEDED(hr))
			hr = adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return;

		//	垂直同期なしで待たずに表示するにはティアリングの許可が要る
		Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory.As(&factory5)))
		{
			BOOL allow_tearing = FALSE;
			if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing))))
				tearing_supported = allow_tearing != FALSE;
		}
		swap_chain_flags = tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		//	フリップモデル
		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
		swap_chain_desc.Width = SCREEN_WIDTH;
		swap_chain_desc.Height = SCREEN_HEIGHT;
		swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.Stereo = FALSE;
		swap_chain_desc.SampleDesc.Count = 1;
		swap_chain_desc.SampleDesc.Quality = 0;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.BufferCount = 2;
		swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swap_chain_desc.Flags = swap_chain_flags;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc{};
		fullscreen_desc.RefreshRate.Numerator = 60;
		fullscreen_desc.RefreshRate.Denominator = 1;
		fullscreen_desc.Windowed = !FULLSCREEN;

		hr = factory->CreateSwapChainForHwnd(device.Get(), hwnd, &swap_chain_desc, &fullscreen_desc, nullptr, swap_chain.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		//	Alt+Enter で排他フルスクリーンに切り替わらないようにする
		factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	}
	if (!swap_chain)
		return;

	//	バックバッファの描画先
	{
		back_buffer_pass = std::make_shared<render_pass>();
		back_buffer_pass->render_targets.emplace_back(swap_chain);
		back_buffer_pass->render_targets.back().clear_color = false;
	}

	state_cache.initialize(device.Get());

	//	VS から描き込み先の配列番号を出力できるかチェック
	{
		D3D11_FEATURE_DATA_D3D11_OPTIONS3 options{};
		hr = device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &options, sizeof(options));
		supported_device = SUCCEEDED(hr) && options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer;
	}
}

render_data::~render_data()
{
}

void render_data::present(UINT sync_interval)
{
	if (!swap_chain)
		return;

	//	作り直し中は表示しない
	if (back_buffer_pass->render_targets.empty())
		return;

	//	ティアリング許可は垂直同期なし・ウィンドウ表示のときだけ使える
	UINT flags = 0;
	if (sync_interval == 0 && tearing_supported)
	{
		BOOL fullscreen = FALSE;
		swap_chain->GetFullscreenState(&fullscreen, nullptr);
		if (!fullscreen)
			flags |= DXGI_PRESENT_ALLOW_TEARING;
	}
	swap_chain->Present(sync_interval, flags);
}

void render_data::release_back_buffer()
{
	back_buffer_pass->render_targets.clear();
}

bool render_data::resize_back_buffer(UINT width, UINT height)
{
	if (!swap_chain)
		return false;

	//	ResizeBuffers はバックバッファへの参照が残っていると失敗するので、
	//	描画先を外して破棄を済ませる
	release_back_buffer();
	immediate_context->OMSetRenderTargets(0, nullptr, nullptr);
	immediate_context->Flush();

	HRESULT hr = swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, swap_chain_flags);
	if (FAILED(hr))
		DEBUG_LOG_CATEGORY(logger::categorys::system, "ResizeBuffers Failed [%u x %u] hr=0x%08X", width, height, static_cast<unsigned int>(hr));

	back_buffer_pass->render_targets.emplace_back(swap_chain);
	back_buffer_pass->render_targets.back().clear_color = false;
	return SUCCEEDED(hr);
}

void render_data::upload_gpu_data()
{
	upload_list.execute(immediate_context.Get());
}

void render_data::polling_gpu_readback()
{
	readback_queue.poll(immediate_context.Get());
}
