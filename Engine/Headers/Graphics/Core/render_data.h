#pragma once

#include <map>
#include <mutex>

#include <dxgi1_2.h>

#include "Graphics/Core/gpu_upload_list.h"
#include "Graphics/Core/gpu_readback_queue.h"
#include "Graphics/Core/render_state_cache.h"

CONST BOOL FULLSCREEN{ FALSE };

struct render_target_view;

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

public:	//	HDR関係の情報
	//	ディスプレイカラースペース
	enum class display_color_spaces
	{
		srgb = 0,	//	sRGB
		bt709,		//	BT.709
		bt2100_pq,	//	BT.2100 PQ
	};

	//	ディスプレイカラースペース情報
	struct display_color_space_datas
	{
		float peak_nits = 100.0f;	// ディスプレイ最大輝度
		float full_frame_nits = 100.0f; // ディスプレイが画面全体を白表示した場合の最大輝度
										// Reference White や UI の白輝度を決定する際の参考値として使用する。
		float black_nits = 0.0f;	// ディスプレイが表示可能な最小輝度[nit]
		float sdr_white_nits = 100.0f;	//	SDR の白の輝度[nit]

		float red_primary[2] = {0.640f, 0.330f};	// ディスプレイの赤色の色度座標
		float green_primary[2] = {0.300f, 0.600f};	// ディスプレイの緑色の色度座標
		float blue_primary[2] = {0.150f, 0.060f};	// ディスプレイの青色の色度座標
		float white_point[2] = {0.3127f, 0.3290f};	// ディスプレイの白色点の色度座標
	};

	display_color_spaces get_display_color_space() const { return display_color_space; }
	const display_color_space_datas& get_display_color_space_data() const { return display_color_space_data; }
	bool is_hdr_output() const { return display_color_space == display_color_spaces::bt2100_pq; }

	//	ウィンドウのあるモニターを調べる。
	bool check_display_color_space(bool enable_hdr);

	//	調べた色空間にスワップチェーンを合わせる
	bool change_display_color_space();

	//	UI と ImGui の描画先。SDR・HDR どちらでも SDR のテクスチャ
	render_target_view get_ui_render_target_view() const;

	//	UI のテクスチャをバックバッファへ合成する用
	ID3D11RenderTargetView* get_back_buffer_target() const { return back_buffer_target.Get(); }
	ID3D11RenderTargetView* get_ui_texture_target() const { return ui_texture_target.Get(); }
	ID3D11ShaderResourceView* get_ui_texture_view() const { return ui_texture_view.Get(); }

private:
	void create_display_targets();
	void release_display_targets();

private:
	CONST HWND hwnd;
	math::uint2 screen_size;
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

	UINT swap_chain_flags = 0;
	bool supported_device = false;
	bool tearing_supported = false;

	render_state_cache state_cache;		//	各種レンダーステートキャッシュ
	gpu_upload_list upload_list;		//	GPU転送情報を纏めて処理する
	gpu_readback_queue readback_queue;	//	GPUから情報を取得する

private:	//	モニターの情報
	Microsoft::WRL::ComPtr<IDXGIFactory1> display_factory;
	HMONITOR display_monitor = nullptr;
	bool display_hdr_enabled = false;
	bool display_checked = false;
	display_color_spaces display_color_space = display_color_spaces::srgb;
	display_color_space_datas display_color_space_data;
	display_color_spaces detected_display_color_space = display_color_spaces::srgb;
	display_color_space_datas detected_display_color_space_data;

private:	//	HDR のときのバックバッファの描画先と、UI を SDR で描くテクスチャ
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> back_buffer_target;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ui_texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ui_texture_view;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> ui_texture_target;
};
