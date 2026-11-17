
#include "Graphics/Core/render_data.h"

#include <vector>

#include <d3d11_1.h>
#include <dxgi1_6.h>

#include "Graphics/Core/render_pass.h"

#pragma comment(lib, "dxgi.lib")

namespace
{
	//	SDR の基準白の輝度[nit]
	constexpr float sdr_reference_white_nits = 100.0f;

	//	HDR でモニターから輝度情報が取得できなかった場合の値
	constexpr float hdr_default_peak_nits = 1000.0f;
	constexpr float hdr_default_full_frame_nits = 400.0f;

	//	Windows の「SDR コンテンツの明るさ」[nit]を取る。取れなければ基準白
	float query_sdr_white_nits(const wchar_t* device_name)
	{
		UINT32 path_count = 0;
		UINT32 mode_count = 0;
		if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) != ERROR_SUCCESS)
			return sdr_reference_white_nits;

		std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
		std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
		if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(), nullptr) != ERROR_SUCCESS)
			return sdr_reference_white_nits;

		for (UINT32 index = 0; index < path_count; ++index)
		{
			const DISPLAYCONFIG_PATH_INFO& path = paths[index];

			//	DXGI の出力と同じモニターか
			DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name{};
			source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
			source_name.header.size = sizeof(source_name);
			source_name.header.adapterId = path.sourceInfo.adapterId;
			source_name.header.id = path.sourceInfo.id;
			if (DisplayConfigGetDeviceInfo(&source_name.header) != ERROR_SUCCESS || wcscmp(source_name.viewGdiDeviceName, device_name) != 0)
				continue;

			//	SDRWhiteLevel は 1000 で 80nit
			DISPLAYCONFIG_SDR_WHITE_LEVEL white_level{};
			white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
			white_level.header.size = sizeof(white_level);
			white_level.header.adapterId = path.targetInfo.adapterId;
			white_level.header.id = path.targetInfo.id;
			if (DisplayConfigGetDeviceInfo(&white_level.header) != ERROR_SUCCESS)
				break;
			return static_cast<float>(white_level.SDRWhiteLevel) / 1000.0f * 80.0f;
		}
		return sdr_reference_white_nits;
	}
}

render_data::render_data(CONST HWND hwnd)
: hwnd(hwnd)
{
	HRESULT hr{ S_OK };

	// デバイス＆スワップチェーン生成
	{
		UINT create_device_flags{ 0 };
#if	defined(_DEBUG)
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
		swap_chain_desc.Width = 0;	//	0指定だとクライアント領域
		swap_chain_desc.Height = 0;	//	0指定だとクライアント領域
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

	state_cache.initialize(device.Get());

	//	VS から描き込み先の配列番号を出力できるかチェック
	{
		D3D11_FEATURE_DATA_D3D11_OPTIONS3 options{};
		hr = device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &options, sizeof(options));
		supported_device = SUCCEEDED(hr) && options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer;
	}

	create_display_targets();
}

render_data::~render_data()
{
}

void render_data::present(UINT sync_interval)
{
	if (!swap_chain)
		return;

	//	ティアリング許可は垂直同期なしのときだけ使う
	UINT flags = 0;
	if (sync_interval == 0 && tearing_supported)
		flags |= DXGI_PRESENT_ALLOW_TEARING;
	swap_chain->Present(sync_interval, flags);
}

bool render_data::resize_back_buffer(UINT width, UINT height)
{
	if (!swap_chain)
		return false;

	release_display_targets();
	immediate_context->OMSetRenderTargets(0, nullptr, nullptr);
	immediate_context->Flush();

	HRESULT hr = swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, swap_chain_flags);
	if (FAILED(hr))
		DEBUG_LOG_CATEGORY(logger::categorys::system, "ResizeBuffers Failed [%u x %u] hr=0x%08X", width, height, static_cast<unsigned int>(hr));
	create_display_targets();
	return SUCCEEDED(hr);
}

bool render_data::check_display_color_space(bool enable_hdr)
{
	if (!swap_chain)
		return false;

	//	ウィンドウがどのモニターに乗ってるか取得
	HMONITOR current_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	const bool factory_current = display_factory && display_factory->IsCurrent();
	if (display_checked && factory_current && current_monitor == display_monitor && enable_hdr == display_hdr_enabled)
		return false;

	display_checked = true;
	display_monitor = current_monitor;
	display_hdr_enabled = enable_hdr;

	//	ディスプレイの構成が変わると古いファクトリーは古い情報を返すので作り直す
	if (!factory_current)
	{
		display_factory.Reset();
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(display_factory.GetAddressOf()))))
			display_factory.Reset();
	}

	//	モニター輝度初期化
	display_color_space_datas data{};
	DXGI_COLOR_SPACE_TYPE color_space_type = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

	//	各モニターから、ウィンドウが乗っているものを探す
	bool found = false;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
	for (UINT adapter_index = 0; display_factory && !found && SUCCEEDED(display_factory->EnumAdapters1(adapter_index, adapter.ReleaseAndGetAddressOf())); ++adapter_index)
	{
		Microsoft::WRL::ComPtr<IDXGIOutput> temp;
		for (UINT output_index = 0; SUCCEEDED(adapter->EnumOutputs(output_index, temp.ReleaseAndGetAddressOf())); ++output_index)
		{
			Microsoft::WRL::ComPtr<IDXGIOutput6> output;
			if (FAILED(temp.As(&output)))
				continue;

			//	モニター判定
			DXGI_OUTPUT_DESC1 desc1;
			if (FAILED(output->GetDesc1(&desc1)) || desc1.Monitor != current_monitor)
				continue;

			color_space_type = desc1.ColorSpace;	//	ディスプレイ設定に合わせて色空間を変える

			data.black_nits = desc1.MinLuminance;
			data.peak_nits = desc1.MaxLuminance;
			data.full_frame_nits = desc1.MaxFullFrameLuminance;
			data.sdr_white_nits = query_sdr_white_nits(desc1.DeviceName);

			//	xy色度図における各色の点
			data.red_primary[0] = desc1.RedPrimary[0];
			data.red_primary[1] = desc1.RedPrimary[1];
			data.green_primary[0] = desc1.GreenPrimary[0];
			data.green_primary[1] = desc1.GreenPrimary[1];
			data.blue_primary[0] = desc1.BluePrimary[0];
			data.blue_primary[1] = desc1.BluePrimary[1];
			data.white_point[0] = desc1.WhitePoint[0];
			data.white_point[1] = desc1.WhitePoint[1];
			found = true;
			break;
		}
	}

	//	ディスプレイの色空間判定
	display_color_spaces color_space = display_color_spaces::srgb;
	if (enable_hdr && color_space_type == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
		color_space = display_color_spaces::bt2100_pq;

	//	ディスプレイ輝度の補正
	if (color_space != display_color_spaces::bt2100_pq)
	{
		data.peak_nits = sdr_reference_white_nits;
		data.full_frame_nits = sdr_reference_white_nits;
		data.sdr_white_nits = sdr_reference_white_nits;
	}
	else
	{
		//	0 のままだとトーンマッピング/OETF でゼロ除算になる
		if (data.peak_nits <= 0.0f)
			data.peak_nits = hdr_default_peak_nits;
		if (data.full_frame_nits <= 0.0f)
			data.full_frame_nits = (std::min)(hdr_default_full_frame_nits, data.peak_nits);
		if (data.sdr_white_nits <= 0.0f)
			data.sdr_white_nits = sdr_reference_white_nits;
	}

	detected_display_color_space = color_space;
	detected_display_color_space_data = data;

	//	色空間が同じなら輝度などの情報だけ差し替える
	if (color_space == display_color_space)
	{
		display_color_space_data = data;
		return false;
	}
	return true;
}

bool render_data::change_display_color_space()
{
	if (!swap_chain)
		return false;

	//	ディスプレイ色空間設定用にスワップチェイン3取得
	Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3;
	if (FAILED(swap_chain.As(&swap_chain3)))
		return false;

	release_display_targets();
	immediate_context->OMSetRenderTargets(0, nullptr, nullptr);
	immediate_context->Flush();

	display_color_spaces color_space = detected_display_color_space;
	display_color_space_datas data = detected_display_color_space_data;

	//	HDR は R10G10B10A2 + PQ + Rec.2020
	//	SDR は R8G8B8A8 + sRGB
	auto apply = [&](display_color_spaces color_space)
	{
		const bool hdr = color_space == display_color_spaces::bt2100_pq;
		const DXGI_FORMAT format = hdr ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
		const DXGI_COLOR_SPACE_TYPE color_space_type = hdr ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

		DXGI_SWAP_CHAIN_DESC1 desc{};
		swap_chain->GetDesc1(&desc);
		HRESULT hr = swap_chain->ResizeBuffers(0, desc.Width, desc.Height, format, swap_chain_flags);
		if (FAILED(hr))
		{
			DEBUG_LOG_CATEGORY(logger::categorys::system, "ResizeBuffers Failed [format %d] hr=0x%08X", static_cast<int>(format), static_cast<unsigned int>(hr));
			return false;
		}

		UINT support = 0;
		if (FAILED(swap_chain3->CheckColorSpaceSupport(color_space_type, &support)) || !(support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
			return false;
		return SUCCEEDED(swap_chain3->SetColorSpace1(color_space_type));
	};
	bool succeeded = apply(color_space);
	if (!succeeded && color_space != display_color_spaces::srgb)
	{
		//	HDR にできなければ SDR に戻す
		DEBUG_LOG_CATEGORY(logger::categorys::system, "HDR output is not supported");
		color_space = display_color_spaces::srgb;
		data.peak_nits = sdr_reference_white_nits;
		data.full_frame_nits = sdr_reference_white_nits;
		data.sdr_white_nits = sdr_reference_white_nits;
		succeeded = apply(color_space);
	}

	display_color_space = color_space;
	display_color_space_data = data;
	create_display_targets();
	return succeeded;
}

render_target_view render_data::get_ui_render_target_view() const
{
	if (!ui_texture_target)
		return render_target_view(swap_chain);

	render_target_view result;
	result.view = ui_texture_target;
	D3D11_TEXTURE2D_DESC desc{};
	ui_texture->GetDesc(&desc);
	result.size = { desc.Width, desc.Height };
	return result;
}

void render_data::create_display_targets()
{
	release_display_targets();
	if (!swap_chain)
		return;

	//	バックバッファ取得
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
	hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return;
	hr = device->CreateRenderTargetView(back_buffer.Get(), nullptr, back_buffer_target.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	//	UI は SDR のまま描くので、バックバッファと同じ大きさの R8G8B8A8 を用意
	D3D11_TEXTURE2D_DESC back_buffer_desc{};
	back_buffer->GetDesc(&back_buffer_desc);
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = back_buffer_desc.Width;
	desc.Height = back_buffer_desc.Height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	hr = device->CreateTexture2D(&desc, nullptr, ui_texture.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return;
	hr = device->CreateShaderResourceView(ui_texture.Get(), nullptr, ui_texture_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	hr = device->CreateRenderTargetView(ui_texture.Get(), nullptr, ui_texture_target.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	const FLOAT clear_color[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
	immediate_context->ClearRenderTargetView(ui_texture_target.Get(), clear_color);
}

void render_data::release_display_targets()
{
	back_buffer_target.Reset();
	ui_texture_target.Reset();
	ui_texture_view.Reset();
	ui_texture.Reset();
}

void render_data::upload_gpu_data()
{
	upload_list.execute(immediate_context.Get());
}

void render_data::polling_gpu_readback()
{
	readback_queue.poll(immediate_context.Get());
}
