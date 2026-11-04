#include "render_state_cache.h"

#include "System/misc.h"

namespace
{
	D3D11_BLEND_DESC make_key(const D3D11_BLEND_DESC& desc)
	{
		D3D11_BLEND_DESC key = {};
		key.AlphaToCoverageEnable = desc.AlphaToCoverageEnable;
		key.IndependentBlendEnable = desc.IndependentBlendEnable;
		for (UINT index = 0; index < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++index)
		{
			const D3D11_RENDER_TARGET_BLEND_DESC& src = desc.RenderTarget[index];
			D3D11_RENDER_TARGET_BLEND_DESC& dst = key.RenderTarget[index];
			dst.BlendEnable = src.BlendEnable;
			dst.SrcBlend = src.SrcBlend;
			dst.DestBlend = src.DestBlend;
			dst.BlendOp = src.BlendOp;
			dst.SrcBlendAlpha = src.SrcBlendAlpha;
			dst.DestBlendAlpha = src.DestBlendAlpha;
			dst.BlendOpAlpha = src.BlendOpAlpha;
			dst.RenderTargetWriteMask = src.RenderTargetWriteMask;
		}
		return key;
	}

	D3D11_DEPTH_STENCIL_DESC make_key(const D3D11_DEPTH_STENCIL_DESC& desc)
	{
		D3D11_DEPTH_STENCIL_DESC key = {};
		key.DepthEnable = desc.DepthEnable;
		key.DepthWriteMask = desc.DepthWriteMask;
		key.DepthFunc = desc.DepthFunc;
		key.StencilEnable = desc.StencilEnable;
		key.StencilReadMask = desc.StencilReadMask;
		key.StencilWriteMask = desc.StencilWriteMask;
		key.FrontFace = desc.FrontFace;
		key.BackFace = desc.BackFace;
		return key;
	}

	D3D11_RASTERIZER_DESC make_key(const D3D11_RASTERIZER_DESC& desc) { return desc; }
	D3D11_SAMPLER_DESC make_key(const D3D11_SAMPLER_DESC& desc) { return desc; }

	HRESULT create_state(ID3D11Device* device, const D3D11_BLEND_DESC& desc, ID3D11BlendState** out) { return device->CreateBlendState(&desc, out); }
	HRESULT create_state(ID3D11Device* device, const D3D11_RASTERIZER_DESC& desc, ID3D11RasterizerState** out) { return device->CreateRasterizerState(&desc, out); }
	HRESULT create_state(ID3D11Device* device, const D3D11_DEPTH_STENCIL_DESC& desc, ID3D11DepthStencilState** out) { return device->CreateDepthStencilState(&desc, out); }
	HRESULT create_state(ID3D11Device* device, const D3D11_SAMPLER_DESC& desc, ID3D11SamplerState** out) { return device->CreateSamplerState(&desc, out); }

	template<class Desc, class State, class Map>
	Microsoft::WRL::ComPtr<State> find_or_create(ID3D11Device* device, Map& states, const Desc& desc)
	{
		const Desc key = make_key(desc);
		auto it = states.find(key);
		if (it != states.end())
			return it->second;

		Microsoft::WRL::ComPtr<State> state;
		HRESULT hr = create_state(device, key, state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return nullptr;

		states.emplace(key, state);
		return state;
	}
}

void render_state_cache::initialize(ID3D11Device* device)
{
	this->device = device;
}

Microsoft::WRL::ComPtr<ID3D11BlendState> render_state_cache::get_blend_state(const D3D11_BLEND_DESC& desc)
{
	std::lock_guard<std::mutex> lock(mutex);
	return find_or_create<D3D11_BLEND_DESC, ID3D11BlendState>(device.Get(), blend_states, desc);
}

Microsoft::WRL::ComPtr<ID3D11RasterizerState> render_state_cache::get_rasterizer_state(const D3D11_RASTERIZER_DESC& desc)
{
	std::lock_guard<std::mutex> lock(mutex);
	return find_or_create<D3D11_RASTERIZER_DESC, ID3D11RasterizerState>(device.Get(), rasterizer_states, desc);
}

Microsoft::WRL::ComPtr<ID3D11DepthStencilState> render_state_cache::get_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC& desc)
{
	std::lock_guard<std::mutex> lock(mutex);
	return find_or_create<D3D11_DEPTH_STENCIL_DESC, ID3D11DepthStencilState>(device.Get(), depth_stencil_states, desc);
}

Microsoft::WRL::ComPtr<ID3D11SamplerState> render_state_cache::get_sampler_state(const D3D11_SAMPLER_DESC& desc)
{
	std::lock_guard<std::mutex> lock(mutex);
	return find_or_create<D3D11_SAMPLER_DESC, ID3D11SamplerState>(device.Get(), sampler_states, desc);
}
