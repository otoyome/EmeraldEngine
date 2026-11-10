#include "Graphics/render_state_cache.h"

#include "Graphics/common_pipeline_states.h"
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
		fixed_sampler_states[static_cast<size_t>(SAMPLER_STATE::POINT)] = get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		fixed_sampler_states[static_cast<size_t>(SAMPLER_STATE::LINEAR)] = get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		fixed_sampler_states[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC)] = get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		fixed_sampler_states[static_cast<size_t>(SAMPLER_STATE::POINT_CLAMP)] = get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		fixed_sampler_states[static_cast<size_t>(SAMPLER_STATE::LINEAR_CLAMP)] = get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		fixed_sampler_states[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC_CLAMP)] = get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.BorderColor[0] = 0;
		sampler_desc.BorderColor[1] = 0;
		sampler_desc.BorderColor[2] = 0;
		sampler_desc.BorderColor[3] = 0;
		fixed_sampler_states[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_BLACK)] = get_sampler_state(sampler_desc);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.BorderColor[0] = 1;
		sampler_desc.BorderColor[1] = 1;
		sampler_desc.BorderColor[2] = 1;
		sampler_desc.BorderColor[3] = 1;
		fixed_sampler_states[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_WHITE)] = get_sampler_state(sampler_desc);
	}

	// 深度ステンシルステートの生成
	{
		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
		depth_stencil_desc.DepthEnable = TRUE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depth_stencil_desc.DepthFunc = common_pipeline_states::depth_comparison_func;
		fixed_depth_stencil_states[static_cast<size_t>(depth_stencil_state::depth_test_and_write)] = get_depth_stencil_state(depth_stencil_desc);
		depth_stencil_desc.DepthEnable = TRUE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = common_pipeline_states::depth_comparison_func;
		fixed_depth_stencil_states[static_cast<size_t>(depth_stencil_state::depth_test_only)] = get_depth_stencil_state(depth_stencil_desc);
		depth_stencil_desc.DepthEnable = FALSE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depth_stencil_desc.DepthFunc = common_pipeline_states::depth_comparison_func;
		fixed_depth_stencil_states[static_cast<size_t>(depth_stencil_state::depth_write_only)] = get_depth_stencil_state(depth_stencil_desc);
		depth_stencil_desc.DepthEnable = FALSE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = common_pipeline_states::depth_comparison_func;
		fixed_depth_stencil_states[static_cast<size_t>(depth_stencil_state::depth_off)] = get_depth_stencil_state(depth_stencil_desc);

		//	デカール用
		{
			D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
			depth_stencil_desc.DepthEnable = TRUE;
			depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			depth_stencil_desc.DepthFunc = common_pipeline_states::depth_comparison_func;
			depth_stencil_desc.StencilEnable = TRUE;
			depth_stencil_desc.StencilReadMask = 0xFF;
			depth_stencil_desc.StencilWriteMask = 0xFF;
			depth_stencil_desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
			depth_stencil_desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
			depth_stencil_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
			depth_stencil_desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
			depth_stencil_desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			depth_stencil_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
			depth_stencil_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
			depth_stencil_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
			fixed_depth_stencil_states[static_cast<size_t>(depth_stencil_state::decal)] = get_depth_stencil_state(depth_stencil_desc);
		}
	}

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		fixed_blend_states[static_cast<size_t>(blend_state::opaque)] = get_blend_state(blend_desc);

		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		fixed_blend_states[static_cast<size_t>(blend_state::translucent)] = get_blend_state(blend_desc);

		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; //D3D11_BLEND_ONE D3D11_BLEND_SRC_ALPHA
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		fixed_blend_states[static_cast<size_t>(blend_state::addative)] = get_blend_state(blend_desc);

		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO; //D3D11_BLEND_DEST_COLOR
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR; //D3D11_BLEND_SRC_COLOR
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		fixed_blend_states[static_cast<size_t>(blend_state::multiply)] = get_blend_state(blend_desc);
	}

	// ラスタライザーステートの生成
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;

		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_BACK;
		fixed_rasterizer_states[static_cast<int>(raster_state::solid)] = get_rasterizer_state(rasterizer_desc);

		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		fixed_rasterizer_states[static_cast<int>(raster_state::two_sided)] = get_rasterizer_state(rasterizer_desc);

		rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
		rasterizer_desc.CullMode = D3D11_CULL_BACK;
		fixed_rasterizer_states[static_cast<int>(raster_state::wire)] = get_rasterizer_state(rasterizer_desc);

		rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		fixed_rasterizer_states[static_cast<int>(raster_state::wire_two_sided)] = get_rasterizer_state(rasterizer_desc);
	}
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
