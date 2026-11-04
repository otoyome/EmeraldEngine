#pragma once

#include <map>
#include <mutex>

#include <d3d11.h>
#include <wrl/client.h>

//	ステートオブジェクトの共有キャッシュ
class render_state_cache
{
	template<class Desc>
	struct desc_less
	{
		bool operator()(const Desc& l, const Desc& r) const { return memcmp(&l, &r, sizeof(Desc)) < 0; }
	};

	template<class Desc, class State>
	using state_map = std::map<Desc, Microsoft::WRL::ComPtr<State>, desc_less<Desc>>;

public:
	render_state_cache() {}
	~render_state_cache() {}

	void initialize(ID3D11Device* device);

	Microsoft::WRL::ComPtr<ID3D11BlendState> get_blend_state(const D3D11_BLEND_DESC& desc);
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> get_rasterizer_state(const D3D11_RASTERIZER_DESC& desc);
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> get_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC& desc);
	Microsoft::WRL::ComPtr<ID3D11SamplerState> get_sampler_state(const D3D11_SAMPLER_DESC& desc);

private:
	Microsoft::WRL::ComPtr<ID3D11Device> device;

	std::mutex mutex;
	state_map<D3D11_BLEND_DESC, ID3D11BlendState> blend_states;
	state_map<D3D11_RASTERIZER_DESC, ID3D11RasterizerState> rasterizer_states;
	state_map<D3D11_DEPTH_STENCIL_DESC, ID3D11DepthStencilState> depth_stencil_states;
	state_map<D3D11_SAMPLER_DESC, ID3D11SamplerState> sampler_states;
};
