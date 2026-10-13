#pragma once

#include <map>
#include <mutex>

CONST LONG SCREEN_WIDTH{ 1280 };
CONST LONG SCREEN_HEIGHT{ 720 };
CONST BOOL FULLSCREEN{ FALSE };

class render_data
{
	SINGLETON_DECREATE(render_data)

public:
	static	inline	void	create_instance(CONST HWND hwnd)
	{
		if(!instance_ptr)
		{
			instance_ptr	= new render_data(hwnd);
		}
	}

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

public:
	CONST HWND get_hwnd() const { return hwnd; }
	Microsoft::WRL::ComPtr<ID3D11Device> get_device() const { return device; }
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> get_immediate_context() const { return immediate_context; }
	Microsoft::WRL::ComPtr<IDXGISwapChain> get_swap_chain() const { return swap_chain; }

	Microsoft::WRL::ComPtr<ID3D11SamplerState> get_sampler_states(SAMPLER_STATE index) const { return sampler_states[static_cast<int>(index)]; }
	Microsoft::WRL::ComPtr<ID3D11SamplerState> get_sampler_state(const D3D11_SAMPLER_DESC& desc);

private:
	struct sampler_desc_less
	{
		bool operator()(const D3D11_SAMPLER_DESC& l, const D3D11_SAMPLER_DESC& r) const { return memcmp(&l, &r, sizeof(D3D11_SAMPLER_DESC)) < 0; }
	};

private:
	render_data(CONST HWND hwnd);
	~render_data();

private:
	CONST HWND hwnd;
	math::uint2 screen_size;
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_states[8];

	std::mutex sampler_state_cache_mutex;
	std::map<D3D11_SAMPLER_DESC, Microsoft::WRL::ComPtr<ID3D11SamplerState>, sampler_desc_less> sampler_state_cache;
};
