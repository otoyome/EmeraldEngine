#pragma once

#include <map>
#include <mutex>

#include "Graphics/gpu_upload_list.h"
#include "Graphics/gpu_readback_queue.h"
#include "render_state_cache.h"

CONST LONG SCREEN_WIDTH{ 1280 };
CONST LONG SCREEN_HEIGHT{ 720 };
CONST BOOL FULLSCREEN{ FALSE };

class render_data
{
	SINGLETON_DECREATE(render_data)

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
	static	inline	void	create_instance(CONST HWND hwnd)
	{
		if(!instance_ptr)
		{
			instance_ptr	= new render_data(hwnd);
		}
	}

public:
	CONST HWND get_hwnd() const { return hwnd; }
	Microsoft::WRL::ComPtr<ID3D11Device> get_device() const { return device; }
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> get_immediate_context() const { return immediate_context; }
	Microsoft::WRL::ComPtr<IDXGISwapChain> get_swap_chain() const { return swap_chain; }

	//	このエンジンの描画に必要な機能を GPU が持っているか
	bool is_supported_device() const { return supported_device; }

	Microsoft::WRL::ComPtr<ID3D11SamplerState> get_sampler_states(SAMPLER_STATE index) const { return sampler_states[static_cast<int>(index)]; }
	Microsoft::WRL::ComPtr<ID3D11SamplerState> get_sampler_state(const D3D11_SAMPLER_DESC& desc) { return state_cache.get_sampler_state(desc); }

	render_state_cache& get_state_cache() { return state_cache; }

	gpu_upload_list& get_upload_list() { return upload_list; }
	gpu_readback_queue& get_readback_queue() { return readback_queue; }

private:
	render_data(CONST HWND hwnd);
	~render_data();

public:
	void upload_gpu_data();
	void polling_gpu_readback();

private:
	CONST HWND hwnd;
	math::uint2 screen_size;
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;
	bool supported_device = false;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_states[8];

	render_state_cache state_cache;

	gpu_upload_list upload_list;
	gpu_readback_queue readback_queue;
};
