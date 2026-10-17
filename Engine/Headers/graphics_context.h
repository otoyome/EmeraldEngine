#pragma once

#include <optional>

#include "light.h"
#include "texture.h"
#include "buffer_view.h"
#include "collision/capsule.h"
#include "collision/box.h"

#include "post_process_volume.h"

#include "common_pipeline_states.h"
#include "Graphics/render_command_recorder.h"
#include "Graphics/render_settings.h"

//	表面描画パス
enum class surface_paths : std::uint8_t
{
	depth_write_pass,	//	深度書き込みパス
	g_buffer_pass,		//	G-Buffer生成パス
	shadow_write_pass,	//	シャドウマップ生成パス
	forward_pass,		//	フォワードレンダリングパス
};

//	半透明 surface 用描画タイミング
enum class translucency_paths : std::uint8_t
{
	before_dof,			//	被写界深度前
	after_dof,			//	被写界深度後
	after_motion_blur,	//	モーションブラー後

	max,

	unknown,
};

//	post process 用描画タイミング
enum class blendable_locations : std::uint8_t
{
	before_dof,				//	被写界深度前
	after_dof,				//	被写界深度後
	after_dof_translucency,	//	被写界深度後半透明描画後

	before_bloom,			//	ブルーム前
	after_tone_mapping,		//	トーンマッピング後

	unknown,
};

//	ライト描画情報
struct light_render_proxy
{
	//	ライト情報
	UINT id = 0;
	light_type type = light_type::directional;
	UINT attribute = 0;
	math::float3 position = {0, 0, 0};
	math::rotatorf rotation = {0, 0, 0};
	math::float3 direction = {0, 0, 1};
	math::float4 color = {1, 1, 1, 1};
	float range = 0.0f;
	float inner_cone = 0.0f;
	float outer_cone = 0.0f;
	light_data data = {};

	//	影関係
	math::float3 shadow_attenuation = {0, 0, 0};
	float csm_distance = 0.0f;
	float pssm_split_weight = 0.0f;
	light::shadow_map_bias_data bias_data[common_pipeline_states::shadow_map_max];

	bool is_flag(light::flags flag) const { return system_utility::is_bitflag(attribute, 1 << static_cast<UINT>(flag)); }
	const light::shadow_map_bias_data& get_shadow_bias(int index) const { return bias_data[math::clamp(index, 0, static_cast<int>(common_pipeline_states::shadow_map_max) - 1)]; }
};

//	ライトプローブ描画情報
struct light_probe_render_proxy
{
	UINT attribute = 0;
	light_probes data;
	texture_ptr diffuse_ibl_texture;
	texture_ptr specular_ibl_texture;
	texture_ptr lut_ggx_texture;

	bool is_flag(light_probe::flags flag) const { return system_utility::is_bitflag(attribute, 1 << static_cast<UINT>(flag)); }
};

//	ライトコンテナ
struct light_container
{
	std::vector<light_render_proxy> lights;
	std::vector<light_probe_render_proxy> light_probes;
};

//	転送データリスト
struct gpu_upload_list
{
	struct command
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		bool use_box = false;				//	範囲指定で書き込むか（偽ならバッファ全体）
		UINT offset_in_bytes = 0;			//	オフセット
		std::vector<uint8_t> owned_data;	//	データ
		const void* data = nullptr;			//	このフレームの描画データが所有するデータ
		UINT size_in_bytes = 0;
	};
	std::vector<command> commands;

	//	データを複製して積む
	void add_copy(const Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, bool use_box, UINT offset_in_bytes, const void* data, UINT size_in_bytes);

	//	データの所有権を移して積む
	void add_owned(const Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, bool use_box, UINT offset_in_bytes, std::vector<uint8_t>&& data);

	//	このフレームの描画データ内のデータを参照して積む
	void add_reference(const Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, bool use_box, UINT offset_in_bytes, const void* data, UINT size_in_bytes);

	//	GPU転送処理
	void upload(ID3D11DeviceContext* context) const;

	//	空判定
	bool empty() const { return commands.empty(); }
};

//	材質
class material_render_proxy;
struct material_datas
{
	std::shared_ptr<const material_render_proxy> material;
};

//	メッシュ描画情報コンテナ
struct mesh_container
{
	//	描画情報
	struct draw_datas
	{
		UINT buffer_index;
		UINT material_index;
	};

	//	カプセルシャドウ情報
	struct capsule_shadow_datas
	{
		collision::bounding_capsule capsule;
	};

	std::vector<buffer_datas> buffer_datas;
	std::vector<material_datas> material_datas;
	std::vector<capsule_shadow_datas> capsule_shadow_datas;
	std::vector<draw_datas> draw_datas;
	std::vector<std::size_t> color_datas;
	std::vector<std::size_t> shadow_datas;

	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_header_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> primitive_header_srv;
	UINT primitive_header_capacity = 0;
	Microsoft::WRL::ComPtr<ID3D11Buffer> bone_matrix_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bone_matrix_srv;
	UINT bone_matrix_capacity = 0;
	std::vector<common_pipeline_states::primitive_header> primitive_header_scratch;
	std::vector<math::float4x4> bone_matrix_scratch;
	std::vector<std::shared_ptr<const void>> keep_alive_objects;

	gpu_upload_list transform_uploads;

	void build_transform_buffers();
	void clear();
};

//	スプライト情報コンテナ
struct sprite_render_proxy;
struct sprite_datas
{
	int sort_key;
	int sort_order;
	std::shared_ptr<const sprite_render_proxy> sprite;
};
struct sprite_container
{
	std::vector<sprite_datas> sprite_datas;
};

//	パーティクル描画プロキシー
struct particle_render_proxy
{
	//	バッファビュー
	struct buffer_view
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
		std::vector<UINT> strides_for_vertex;
		std::vector<UINT> offsets_for_vertex;

		Microsoft::WRL::ComPtr<ID3D11Buffer> indirect_buffer;
		UINT offset_for_args = 0;

		buffer_view() = default;
		~buffer_view() {}
	};

	buffer_view vertex_buffer_view;	//	頂点バッファビュー
	material_datas material_data;	//	マテリアル情報
	D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

//	エフェクト情報コンテナ
struct effect_container
{
	std::vector<particle_render_proxy> particle_proxies;
	std::vector<std::size_t> particle_color_datas;
	std::vector<std::size_t> particle_shadow_datas;

	void clear()
	{
		particle_proxies.clear();
		particle_color_datas.clear();
		particle_shadow_datas.clear();
	}
};

//	デカール情報
//	@remaidner	時間が無いのでレンダラー側にモデルを持たせている
struct decal_render_proxy
{
	math::float3 scale = {1, 1, 1};
	math::rotatorf rotation = {0, 0, 0};
	math::float3 position = {0, 0, 0};
	collision::bounding_box box;
	material_datas material;
};
struct decal_container
{
	std::vector<decal_render_proxy> proxies;
	void clear()
	{
		proxies.clear();
	}
};

#if defined(USE_IMGUI)
//	デバッグ描画情報コンテナ
struct debug_draw_primitive_container
{
	enum class line_buffer_id
	{
		normal,
		disable_depth_test,

		max,
	};
	buffer_datas line_buffer_datas[static_cast<int>(line_buffer_id::max)];
	std::vector<buffer_datas::sub_mesh> line_meshes[static_cast<int>(line_buffer_id::max)];
	std::unordered_map<semantic, buffer_view> line_vertex_buffer_views[static_cast<int>(line_buffer_id::max)];
};
#endif // defined(USE_IMGUI)

//	描画データコンテキスト
struct render_data_context
{
	math::vector3d render_origin;	//	描画データ収集時の描画原点
	render_settings settings;		//	描画設定
	gpu_upload_list upload_list;	//	描画データのGPU転送リスト

	//	各種描画コンテナ
	light_container light_container;
	post_process_volume_container post_process_volume_container;
	mesh_container mesh_container;
	sprite_container sprite_container;
	effect_container effect_container;
	decal_container decal_container;
#if defined(USE_IMGUI)
	debug_draw_primitive_container debug_draw_container;
#endif // defined(USE_IMGUI)
};

class clustered_lighting_system;

//	描画コンテキスト
struct graphics_context
{
	std::shared_ptr<render_data_context> render_data;	//	描画データ
	render_command_recorder recorder;	//	描画コマンドレコーダー

	//	リソースバインドテーブル
	resource_view_bind_table bind_table;

	//	シーン情報
	common_pipeline_states::scene_constants scene_constant;
	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;

	//	シャドウマップ用情報
	bool is_shadow_rendering = false;	//	影生成描画処理中判定

	//	強制設定フラグ
	bool is_force_material_wire_draw = false;
	bool is_force_material_both_draw = false;

	//	クラスタードライティング
	std::shared_ptr<clustered_lighting_system> clustered_lighting;

public:
	//	バインドテーブルにフレーム・視点層のリソースを登録
	void register_frame_bind_table();

public:
	//	パイプラインステート登録
	void set_pipeline(const graphics_pipeline_state& pipeline) const { recorder.set_pipeline(pipeline, bind_table); }

	//	描画コマンド登録
	void bind_draw(const graphics_pipeline_state& pipeline, const material_render_proxy& material, const buffer_datas* buffer_data) const
	{
		render_state_overrides overrides;
		overrides.is_shadow_rendering = is_shadow_rendering;
		overrides.force_wire = is_force_material_wire_draw;
		overrides.force_two_sided = is_force_material_both_draw;
		recorder.bind_draw(pipeline, bind_table, material, buffer_data, overrides);
	}
	void end_pipeline() const { recorder.end_pipeline(); }

public:
	//	@tool 以下は recorder への中継（既存の呼び出し元のため）
	//	ラスタライザーステート設定
	void set_rasterizer_state(ID3D11RasterizerState* state) const { recorder.set_rasterizer_state(state); }

	//	深度ステンシルステート設定
	void set_depth_stencil_state(ID3D11DepthStencilState* state, UINT stencil_ref) const { recorder.set_depth_stencil_state(state, stencil_ref); }

	//	ブレンドステート設定
	void set_blend_state(ID3D11BlendState* state) const { recorder.set_blend_state(state); }

	//	図形設定
	void set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY topology) const { recorder.set_primitive_topology(topology); }

	//	頂点バッファ設定
	void set_vertex_buffers(UINT start_slot, UINT num_buffers, ID3D11Buffer* const* buffers, const UINT* strides, const UINT* offsets) const { recorder.set_vertex_buffers(start_slot, num_buffers, buffers, strides, offsets); }

	//	インデックスバッファ設定
	void set_index_buffer(ID3D11Buffer* buffer, DXGI_FORMAT format, UINT offset) const { recorder.set_index_buffer(buffer, format, offset); }

	//	設定していたシェーダーを全て解除する
	void clear_shader_slots() const { recorder.clear_shader_slots(); }

	//	設定されている頂点バッファを外す
	void clear_vertex_buffer() const { recorder.clear_vertex_buffer(); }

	//	設定されているインデックスバッファを外す
	void clear_index_buffer() const { recorder.clear_index_buffer(); }

	//	描画ターゲットのクリア
	void clear_render_target_slots() const { recorder.clear_render_target_slots(); }

	//	CBVのクリア設定
	void clear_constant_buffers(int start_slot = 0, int num = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) const { recorder.clear_constant_buffers(start_slot, num); }

	//	設定していたSRVを全て解除する
	void clear_shader_resorce_views(int start_slot = 0, int num = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) const { recorder.clear_shader_resorce_views(start_slot, num); }

	//	設定していたUAVを全て解除する
	//	FEATURELEVELが11.1ならnumはD3D11_1_UAV_SLOT_COUNTになる
	void clear_unordered_access_views(int start_slot = 0, int num = 8) const { recorder.clear_unordered_access_views(start_slot, num); }

	//	設定していたSamplerを全て解除する
	void clear_sampler(int start_slot = 0, int num = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) const { recorder.clear_sampler(start_slot, num); }

	//	デバッグイベント開始
	void debug_begin_event(const wchar_t* event_name);

	//	デバッグイベント終了
	void debug_end_event();
};
