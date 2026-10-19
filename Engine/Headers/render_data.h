#pragma once

#include <map>
#include <mutex>
#include <functional>

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

	//	マルチスレッド描画に関わるデバイスの対応状況
	struct feature_supports
	{
		bool driver_concurrent_creates = false;						//	リソース生成をドライバーが並列に処理できるか
		bool driver_command_lists = false;							//	コマンドリストをドライバーが直接扱えるか
		bool constant_buffer_offsetting = false;					//	*SetConstantBuffers1 で定数バッファの一部を割り当てられるか
		bool map_no_overwrite_on_dynamic_constant_buffer = false;	//	DYNAMIC 定数バッファに NO_OVERWRITE で Map できるか
	};
	const feature_supports& get_feature_supports() const { return features; }

	//	@tool immediate context の排他（提出スレッドはフレームを提出している間ずっと保持する）
	//	@tool 他のスレッドから immediate context を使う場合（読み戻し・転送・コピーなど）は、これを取ってから使う
	std::recursive_mutex& get_immediate_context_mutex() { return immediate_context_mutex; }

	//	@tool 描画中のフレームが全て提出し終わるまで待つ（描画側が参照している資源を書き換える前に、メインスレッドから呼ぶ）
	void set_flush_rendering_function(std::function<void()> function) { flush_rendering_function = std::move(function); }
	void flush_rendering() const { if (flush_rendering_function) flush_rendering_function(); }

	//	@tool 描画中のフレームが使い終わってから破棄する（差し替えた古い中身を渡す。メインスレッドから呼ぶ）
	//	@tool 次に抽出するフレームに持たせ、そのフレームの提出が終わった後にメインスレッドで破棄する（描画スレッドが動いていない時はすぐ破棄する）
	void set_retire_function(std::function<void(std::shared_ptr<const void>)> function) { retire_function = std::move(function); }
	void retire_after_rendering(std::shared_ptr<const void> object) const { if (retire_function) retire_function(std::move(object)); }

private:
	//	@tool デバイスの対応状況を調べてログに出す
	void check_feature_supports();

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

	feature_supports features;

	std::recursive_mutex immediate_context_mutex;
	std::function<void()> flush_rendering_function;
	std::function<void(std::shared_ptr<const void>)> retire_function;

	std::mutex sampler_state_cache_mutex;
	std::map<D3D11_SAMPLER_DESC, Microsoft::WRL::ComPtr<ID3D11SamplerState>, sampler_desc_less> sampler_state_cache;
};
