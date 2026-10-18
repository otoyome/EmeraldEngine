#include "Editor/imgui_present.h"

#if defined(USE_IMGUI)
#include <algorithm>

#include "render_data.h"
#include "imgui/imgui_impl_dx11.h"
#include "Editor/detail_profiler.h"

namespace
{
	//	@tool スワップチェーンのバックバッファから描画先ビューを作る
	void create_viewport_render_target_view(IDXGISwapChain* swap_chain, Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& render_target_view)
	{
		render_target_view.Reset();
		Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
		if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()))))
			return;
		render_data::instance()->get_device()->CreateRenderTargetView(back_buffer.Get(), nullptr, render_target_view.GetAddressOf());
	}
}

namespace imgui_present
{
	draw_snapshot::~draw_snapshot()
	{
		for (ImDrawList* list : draw_data.CmdLists)
		{
			IM_DELETE(list);
		}
	}

	draw_snapshot_ptr copy_draw_data(const ImDrawData& source)
	{
		auto snapshot = std::make_shared<draw_snapshot>();
		snapshot->draw_data = source;
		snapshot->draw_data.CmdLists.resize(0);
		for (ImDrawList* list : source.CmdLists)
		{
			snapshot->draw_data.CmdLists.push_back(list->CloneOutput());

			//	@tool 参照しているテクスチャ（DX11 では SRV）を保持する
			for (const ImDrawCmd& command : list->CmdBuffer)
			{
				auto* view = static_cast<ID3D11ShaderResourceView*>(command.GetTexID());
				if (!view)
					continue;
				if (std::find_if(snapshot->textures.begin(), snapshot->textures.end(), [view](const auto& held) { return held.Get() == view; }) == snapshot->textures.end())
					snapshot->textures.emplace_back(view);
			}
		}
		return snapshot;
	}

	std::shared_ptr<present_target> present_target::create(HWND window, UINT width, UINT height)
	{
		auto target = std::make_shared<present_target>();
		target->width = width;
		target->height = height;

		ID3D11Device* device = render_data::instance()->get_device().Get();
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		Microsoft::WRL::ComPtr<IDXGIFactory> factory;
		if (window &&
			SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(dxgi_device.GetAddressOf()))) &&
			SUCCEEDED(dxgi_device->GetAdapter(adapter.GetAddressOf())) &&
			SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()))))
		{
			DXGI_SWAP_CHAIN_DESC desc = {};
			desc.BufferDesc.Width = width;
			desc.BufferDesc.Height = height;
			desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.BufferCount = 1;
			desc.OutputWindow = window;
			desc.Windowed = TRUE;
			desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			//	@tool スワップチェーンの生成は immediate context を使う事があるので、提出スレッドと排他する
			std::lock_guard<std::recursive_mutex> immediate_lock(render_data::instance()->get_immediate_context_mutex());
			if (SUCCEEDED(factory->CreateSwapChain(device, &desc, target->swap_chain.GetAddressOf())))
				create_viewport_render_target_view(target->swap_chain.Get(), target->render_target_view);
		}
		return target;
	}

	void present_target::resize(UINT new_width, UINT new_height)
	{
		if (!swap_chain)
			return;

		std::lock_guard<std::recursive_mutex> immediate_lock(render_data::instance()->get_immediate_context_mutex());
		render_target_view.Reset();
		if (FAILED(swap_chain->ResizeBuffers(0, new_width, new_height, DXGI_FORMAT_UNKNOWN, 0)))
			return;
		width = new_width;
		height = new_height;
		create_viewport_render_target_view(swap_chain.Get(), render_target_view);
	}

	void present_target::release()
	{
		std::lock_guard<std::recursive_mutex> immediate_lock(render_data::instance()->get_immediate_context_mutex());
		render_target_view.Reset();
		swap_chain.Reset();
	}

	void draw_windows(ID3D11DeviceContext* immediate_context, const std::vector<window_draw>& windows)
	{
		if (windows.empty())
			return;

		for (const window_draw& window : windows)
		{
			ID3D11RenderTargetView* render_target_view = window.target->render_target_view.Get();
			if (!render_target_view)
				continue;
			immediate_context->OMSetRenderTargets(1, &render_target_view, nullptr);
			if (window.clear)
			{
				const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
				immediate_context->ClearRenderTargetView(render_target_view, clear_color);
			}
			ImGui_ImplDX11_RenderDrawData(&window.draw_data->draw_data);
			window.target->swap_chain->Present(0, 0);
		}
		//	@tool 表示先のバッファを外しておく（メインスレッドでリサイズする際に参照が残らないように）
		immediate_context->OMSetRenderTargets(0, nullptr, nullptr);
	}
}
#endif	//	defined(USE_IMGUI)
