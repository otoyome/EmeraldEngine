#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <d3d11.h>
#include <wrl.h>

#include "graphics_context.h"
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
	void setup_frame(const render_data_context& render_data, const common_pipeline_states::scene_constants& scene);

	//	クラスター構築
	void build(graphics_context& context);

	//	クラスター内のライトでライティングする
	void render(graphics_context& context);

public:
	//	クラスタードライティングが有効か
	static bool is_enable();

	//	クラスタードライティングで描画するライトか
	static bool is_target(const light_render_data& light);

	//	ライト数のヒートマップ表示中か
	static bool is_visualize();

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
	std::shared_ptr<pixel_shader> visualize_ps;

	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state;
	Microsoft::WRL::ComPtr<ID3D11BlendState> visualize_blend_state;
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
	Microsoft::WRL::ComPtr<ID3D11Buffer> statistics_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> statistics_uav;
	std::shared_ptr<std::atomic<UINT>> required_index_count = std::make_shared<std::atomic<UINT>>(0);

	//	今回の描画情報
	UINT light_count = 0;
};
