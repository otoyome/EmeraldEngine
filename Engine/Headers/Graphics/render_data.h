#pragma once

#include <map>
#include <mutex>

#include <dxgi1_2.h>

#include "Graphics/gpu_upload_list.h"
#include "Graphics/gpu_readback_queue.h"
#include "Graphics/render_state_cache.h"

CONST LONG SCREEN_WIDTH{ 1280 };
CONST LONG SCREEN_HEIGHT{ 720 };
CONST BOOL FULLSCREEN{ FALSE };

struct render_pass;

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
	CONST HWND get_hwnd() const { return hwnd; }
	Microsoft::WRL::ComPtr<ID3D11Device> get_device() const { return device; }
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> get_immediate_context() const { return immediate_context; }
	Microsoft::WRL::ComPtr<IDXGISwapChain1> get_swap_chain() const { return swap_chain; }
	std::shared_ptr<const render_pass> get_back_buffer_pass() const { return back_buffer_pass; }

	render_state_cache& get_state_cache() { return state_cache; }
	gpu_upload_list& get_upload_list() { return upload_list; }
	gpu_readback_queue& get_readback_queue() { return readback_queue; }

	bool is_supported_device() const { return supported_device; }

private:
	render_data(CONST HWND hwnd);
	~render_data();

public:
	void upload_gpu_data();
	void polling_gpu_readback();

	void present(UINT sync_interval);

	bool resize_back_buffer(UINT width, UINT height);

private:
	CONST HWND hwnd;
	math::uint2 screen_size;
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
	std::shared_ptr<render_pass> back_buffer_pass;

	UINT swap_chain_flags = 0;
	bool supported_device = false;
	bool tearing_supported = false;

	render_state_cache state_cache;		//	各種レンダーステートキャッシュ
	gpu_upload_list upload_list;		//	GPU転送情報を纏めて処理する
	gpu_readback_queue readback_queue;	//	GPUから情報を取得する
};
