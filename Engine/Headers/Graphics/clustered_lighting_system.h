#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <d3d11.h>
#include <wrl.h>

#include "render_job_context.h"
#include "Graphics/gpu_readback.h"
#include "shader.h"

//	クラスタードライティングシステム
class clustered_lighting_system
{
	struct clustered_constants
	{
		math::uint3 cluster_count;
		UINT light_count;
		math::float2 screen_size;
		float tile_size;
		UINT index_capacity;
		float cluster_near;
		float cluster_far;
		float slice_scale;
		float slice_bias;
		UINT flags;				//	clustered_flag_xxx
		float visualize_max;	//	ヒートマップで最大色になるライト数
		math::float2 dummy;
	};

public:
	//	各種バッファ類取得
	ID3D11Buffer* get_constant_buffer() const { return constant_buffer.Get(); }
	ID3D11ShaderResourceView* get_light_srv() const { return light_srv.Get(); }
	ID3D11ShaderResourceView* get_count_srv() const { return count_srv.Get(); }
	ID3D11ShaderResourceView* get_offset_srv() const { return offset_srv.Get(); }
	ID3D11ShaderResourceView* get_index_srv() const { return index_srv.Get(); }

	//	今回の対象ライト数
	UINT get_light_count() const { return light_count; }

public:
	clustered_lighting_system();
	~clustered_lighting_system();

	//	対象ライトの収集とバッファの準備
	void prepare(const view_render_data& render_data, const common_pipeline_states::scene_constants& scene);

	//	クラスター構築
	void build(render_job_context& context);

	//	クラスター内のライトでライティングする
	void render(render_job_context& context);

	//	前回以前のライト番号総数を読み出す
	void read_statistics();

public:
	//	@tool 現在のコンソール変数の値を写す（メインスレッドで、描画データの抽出時に呼ぶ）
	static void capture_settings(render_settings::clustered_lighting_settings& out_settings);

	//	クラスタードライティングで描画するライトか
	//	@tool 設定は抽出時点の値（view_render_data::settings）を使う
	static bool is_target(const light_render_proxy& light, const render_settings& settings);

	//	@tool クラスターごとのライト数をヒートマップで表示するか（デバッグ機能。USE_IMGUI が無い構成では常に false）
	static bool is_visualize(const render_settings& settings);

private:
	void generate_light_buffer(UINT count);
	void generate_cluster_buffers(UINT count);
	void generate_tile_buffer(UINT count);
	void generate_index_buffer(UINT capacity);

private:
	std::shared_ptr<compute_shader> bounds_cs;
	std::shared_ptr<compute_shader> count_cs;
	std::shared_ptr<compute_shader> scan_cs;
	std::shared_ptr<compute_shader> fill_cs;
	std::shared_ptr<compute_shader> depth_cs;
	std::shared_ptr<vertex_shader> fullscreen_vs;
	std::shared_ptr<pixel_shader> lighting_ps;

	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_state;

	//	定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	clustered_constants constants{};

	//	ライトリスト
	Microsoft::WRL::ComPtr<ID3D11Buffer> light_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> light_srv;
	UINT light_capacity = 0;
	std::vector<light_data> lights;

	//	クラスター
	Microsoft::WRL::ComPtr<ID3D11Buffer> bounds_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bounds_srv;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> bounds_uav;
	Microsoft::WRL::ComPtr<ID3D11Buffer> count_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> count_srv;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> count_uav;
	Microsoft::WRL::ComPtr<ID3D11Buffer> offset_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> offset_srv;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> offset_uav;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cursor_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> cursor_uav;
	UINT cluster_capacity = 0;
	UINT cluster_total = 0;

	//	タイルごとの最も奥の深度分割
	Microsoft::WRL::ComPtr<ID3D11Buffer> tile_depth_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tile_depth_srv;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> tile_depth_uav;
	UINT tile_capacity = 0;
	UINT tile_total = 0;

	//	ライト番号リスト
	Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> index_srv;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> index_uav;
	UINT index_capacity = 0;

	//	ライト番号総数の読み戻し
	static constexpr UINT readback_count = 3;
	Microsoft::WRL::ComPtr<ID3D11Buffer> statistics_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> statistics_uav;
	gpu_readback_buffer statistics_readback;	//	@tool ライト番号総数の読み戻し先
	std::shared_ptr<std::atomic<UINT>> received_index_count = std::make_shared<std::atomic<UINT>>(0);	//	@tool 読み戻しで届いた必要数の最大（提出スレッドで書き込まれる）
	UINT required_index_count = 0;

	//	今回の描画情報
	UINT light_count = 0;

#if defined(USE_IMGUI)
	//	@tool ヒートマップ表示（デバッグ機能）用のシェーダーとブレンドステート
	std::shared_ptr<pixel_shader> visualize_ps;
	Microsoft::WRL::ComPtr<ID3D11BlendState> visualize_blend_state;
#endif	//	defined(USE_IMGUI)
};
