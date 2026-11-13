#pragma once

#include <map>
#include <mutex>

#include <d3d11.h>
#include <wrl/client.h>

//	レンダーステートオブジェクトの共有キャッシュ
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
	enum class SAMPLER_STATE
	{
		POINT,
		LINEAR,
		ANISOTROPIC,
		POINT_CLAMP,
		LINEAR_CLAMP,
		ANISOTROPIC_CLAMP,
		LINEAR_BORDER_BLACK,
		LINEAR_BORDER_WHITE
	};

	enum class depth_stencil_state
	{
		depth_test_and_write,
		depth_test_only,
		depth_write_only,
		depth_off,

		decal,
	};

	enum class blend_state
	{
		opaque,			//	不透明
		translucent,	//	半透明
		addative,		//	加算
		multiply,		//	乗算
	};

	enum class raster_state
	{
		solid,
		two_sided,
		wire,
		wire_two_sided,
	};

public:
	render_state_cache() {}
	~render_state_cache() {}

	void initialize(ID3D11Device* device);

	Microsoft::WRL::ComPtr<ID3D11BlendState> get_blend_state(const D3D11_BLEND_DESC& desc);
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> get_rasterizer_state(const D3D11_RASTERIZER_DESC& desc);
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> get_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC& desc);
	Microsoft::WRL::ComPtr<ID3D11SamplerState> get_sampler_state(const D3D11_SAMPLER_DESC& desc);

	const Microsoft::WRL::ComPtr<ID3D11SamplerState>& get_sampler_state(SAMPLER_STATE index) const { return fixed_sampler_states[static_cast<size_t>(index)]; }
	const Microsoft::WRL::ComPtr<ID3D11DepthStencilState>& get_depth_stencil_state(depth_stencil_state index) const { return fixed_depth_stencil_states[static_cast<size_t>(index)]; }
	const Microsoft::WRL::ComPtr<ID3D11BlendState>& get_blend_state(blend_state index) const { return fixed_blend_states[static_cast<size_t>(index)]; }
	const Microsoft::WRL::ComPtr<ID3D11RasterizerState>& get_rasterizer_state(raster_state index) const { return fixed_rasterizer_states[static_cast<size_t>(index)]; }

private:
	Microsoft::WRL::ComPtr<ID3D11Device> device;

	std::mutex mutex;
	state_map<D3D11_BLEND_DESC, ID3D11BlendState> blend_states;
	state_map<D3D11_RASTERIZER_DESC, ID3D11RasterizerState> rasterizer_states;
	state_map<D3D11_DEPTH_STENCIL_DESC, ID3D11DepthStencilState> depth_stencil_states;
	state_map<D3D11_SAMPLER_DESC, ID3D11SamplerState> sampler_states;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> fixed_sampler_states[magic_enum::enum_count<SAMPLER_STATE>()];
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> fixed_depth_stencil_states[magic_enum::enum_count<depth_stencil_state>()];
	Microsoft::WRL::ComPtr<ID3D11BlendState> fixed_blend_states[magic_enum::enum_count<blend_state>()];
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> fixed_rasterizer_states[magic_enum::enum_count<raster_state>()];
};
