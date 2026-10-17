#pragma once

#include "Graphics/immediate_constant_allocator.h"
#include "Graphics/binding_table.h"
#include "Graphics/pipeline_layout.h"

#include <array>
#include <cstdint>
#include <unordered_map>

class graphics_pipeline_state;
class material_render_proxy;
struct buffer_datas;

//	描画ステートの上書き設定
struct render_state_overrides
{
	bool is_shadow_rendering = false;	//	シャドウマップ生成中（不透明・深度書き込みにする）
	bool force_wire = false;			//	ワイヤーフレームにする
	bool force_two_sided = false;		//	両面描画にする
};

//	描画コマンドレコーダー
class render_command_recorder
{
public:
	//	記録の開始
	void begin(ID3D11DeviceContext* device_context, ID3D11DeviceContext1* device_context1, immediate_constant_allocator* constant_allocator);

	ID3D11DeviceContext* get_device_context() const { return device_context; }

public:
	//	ステート設定（同じものの再設定は省く）
	void set_rasterizer_state(ID3D11RasterizerState* state) const;
	void set_depth_stencil_state(ID3D11DepthStencilState* state, UINT stencil_ref) const;
	void set_blend_state(ID3D11BlendState* state) const;
	void set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY topology) const;
	void set_vertex_buffers(UINT start_slot, UINT num_buffers, ID3D11Buffer* const* buffers, const UINT* strides, const UINT* offsets) const;
	void set_index_buffer(ID3D11Buffer* buffer, DXGI_FORMAT format, UINT offset) const;

	//	解除
	void clear_shader_slots() const;
	void clear_vertex_buffer() const;
	void clear_index_buffer() const;
	void clear_render_target_slots() const;
	void clear_constant_buffers(int start_slot, int num) const;
	void clear_shader_resorce_views(int start_slot, int num) const;
	void clear_unordered_access_views(int start_slot, int num) const;
	void clear_sampler(int start_slot, int num) const;

public:
	//	定数を揮発性定数バッファへ書き込み、その範囲を返す
	constant_range upload_constants(const void* data, UINT size) const;

	//	リソースバインドレコードを定数バッファへの登録情報を設定
	bool resolve_constant_buffer(const resource_view_bind_entry& entry, bool has_draw_data, constant_range& out_range) const;

	//	@tool 定数バッファの割り当て（範囲指定あり / バッファ全体）
	void set_constant_buffer(shader_stage stage, UINT slot, const constant_range& range) const;
	void set_constant_buffer(shader_stage stage, UINT slot, ID3D11Buffer* buffer) const;

	//	@tool シェーダーリソース・サンプラーの割り当て
	void set_shader_resource_view(shader_stage stage, UINT slot, ID3D11ShaderResourceView* view) const;
	void set_sampler(shader_stage stage, UINT slot, ID3D11SamplerState* sampler) const;

public:
	//	@tool パイプラインの解釈
	//	@tool set_pipeline → (描画毎に bind_draw → 描画) → end_pipeline の順に呼ぶ

	//	@tool シェーダー・固定のステートを設定し、割り当て表と固定のリソースを割り当てる
	void set_pipeline(const graphics_pipeline_state& pipeline, const resource_view_bind_table& bindings) const;

	//	@tool 描画毎の設定（マテリアル由来のステート、割り当て表 → マテリアルの順の割り当て、パラメーター定数、描画単位の定数）
	void bind_draw(const graphics_pipeline_state& pipeline, const resource_view_bind_table& bindings, const material_render_proxy& material, const buffer_datas* buffer_data, const render_state_overrides& overrides) const;

	//	@tool パイプラインの使用終了（割り当てを全て外す）
	void end_pipeline() const;

private:
	//	@tool マテリアルの設定からステートを選んで設定する
	void apply_material_render_states(const graphics_pipeline_state& pipeline, const material_render_proxy& material, const render_state_overrides& overrides) const;

private:
	ID3D11DeviceContext* device_context = nullptr;
	ID3D11DeviceContext1* device_context1 = nullptr;
	immediate_constant_allocator* constant_allocator = nullptr;

	//	キャッシュ情報
	mutable ID3D11RasterizerState* cache_rasterizer_state = nullptr;
	mutable ID3D11DepthStencilState* cache_depth_stencil_state = nullptr;
	mutable ID3D11BlendState* cache_blend_state = nullptr;
	mutable UINT cache_stencil_ref = 0xFFFFFFFF;
	mutable D3D_PRIMITIVE_TOPOLOGY cache_primitive_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	mutable UINT cache_vertex_buffer_start_slot = 0xFFFFFFFF;
	mutable UINT cache_vertex_buffer_count = 0;
	mutable ID3D11Buffer* cache_vertex_buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	mutable UINT cache_vertex_buffer_strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	mutable UINT cache_vertex_buffer_offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	mutable ID3D11Buffer* cache_index_buffer = nullptr;
	mutable DXGI_FORMAT cache_index_buffer_format = DXGI_FORMAT_UNKNOWN;
	mutable UINT cache_index_buffer_offset = 0xFFFFFFFF;

	//	@tool パラメーター定数の使い回し
	//	@tool 同じ記録の中で、マテリアル・パイプライン・割り当て表の内容が同じならパラメーター定数も同じなので、書き込んだ範囲をそのまま割り当てる
	struct parameter_cache_key
	{
		const void* material = nullptr;
		const void* pipeline = nullptr;
		const void* bindings = nullptr;
		UINT bindings_revision = 0;
		bool operator==(const parameter_cache_key& other) const
		{
			return material == other.material && pipeline == other.pipeline && bindings == other.bindings && bindings_revision == other.bindings_revision;
		}
	};
	struct parameter_cache_key_hash
	{
		std::size_t operator()(const parameter_cache_key& key) const
		{
			std::size_t hash = std::hash<const void*>()(key.material);
			hash ^= std::hash<const void*>()(key.pipeline) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<const void*>()(key.bindings) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<UINT>()(key.bindings_revision) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			return hash;
		}
	};
	mutable std::unordered_map<parameter_cache_key, std::array<constant_range, graphics_shader_stage_count>, parameter_cache_key_hash> parameter_cache;

	//	@tool 記録器を通して設定した現在の割り当て（シェーダー段・番号ごと）
	//	@tool set_pipeline から end_pipeline までの間だけ信用する（その外では記録器を通さない設定や、描画先の変更による暗黙の解除があるため）
	//	@tool 信用しない間は unknown_binding にしておき、必ず設定する
	static inline const void* const unknown_binding = reinterpret_cast<const void*>(~std::uintptr_t(0));
	//	@tool keep_frame_layer が真なら、フレーム・視点層の番号は覚えたままにする
	void invalidate_bind_table(bool keep_frame_layer) const;

	//	@tool フレーム・視点層の番号（ジョブの間ずっと割り当てたままにする。描画先にならない資源だけを置く）
	//	@tool 定数バッファ b0, b12-b13 / シェーダーリソース t96-t127 / サンプラー s14-s15（グラフィックスの段のみ）
	static constexpr bool is_frame_layer_constant_buffer(UINT slot) { return slot == 0 || slot >= 12; }
	static constexpr bool is_frame_layer_shader_resource(UINT slot) { return slot >= 96; }
	static constexpr bool is_frame_layer_sampler(UINT slot) { return slot >= 14; }
	mutable const pipeline_layout* current_layout = nullptr;
	static constexpr UINT recorded_stage_count = 6;
	struct bound_constant_buffer
	{
		const void* buffer = nullptr;
		UINT first_constant = 0;
		UINT num_constants = 0;
	};
	mutable std::array<std::array<bound_constant_buffer, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT>, recorded_stage_count> bound_constant_buffers{};
	mutable std::array<std::array<const void*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>, recorded_stage_count> bound_shader_resources{};
	mutable std::array<std::array<const void*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT>, recorded_stage_count> bound_samplers{};
};
