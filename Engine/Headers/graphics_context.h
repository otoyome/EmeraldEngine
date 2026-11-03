#pragma once

#include "light.h"
#include "texture.h"
#include "buffer_view.h"
#include "collision/capsule.h"
#include "collision/box.h"

#include "post_process_volume.h"

#include "common_pipeline_states.h"

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
struct light_render_data
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
struct light_probe_render_data
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
	std::vector<light_render_data> lights;
	std::vector<light_probe_render_data> light_probes;
};

//	材質
class material;
class material_instance;
struct material_datas
{
	material_instance* material;
};

//	メッシュ描画情報コンテナ
struct mesh_container
{
	//	カプセルシャドウ情報
	struct capsule_shadow_datas
	{
		collision::bounding_capsule capsule;
	};
	std::vector<buffer_datas> buffer_datas;
	std::vector<material_datas> material_datas;
	std::vector<capsule_shadow_datas> capsule_shadow_datas;
	//	描画情報
	struct draw_datas
	{
		UINT buffer_index;
		UINT material_index;
	};
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

	void build_transform_buffers();

	void clear()
	{
		draw_datas.clear();
		shadow_datas.clear();
		color_datas.clear();
		primitive_header_scratch.clear();
		bone_matrix_scratch.clear();
	}
};

//	スプライト情報コンテナ
class sprite;
struct sprite_datas
{
	int sort_key;
	int sort_order;
	sprite* sprite;
};
struct sprite_container
{
	std::vector<sprite_datas> sprite_datas;
};

//	エフェクト情報コンテナ
struct effect_container
{
	//	パーティクルバッファ情報
	struct sprite_trail
	{
		//	バッファビュー
		struct buffer_view
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertex_stream;
			Microsoft::WRL::ComPtr<ID3D11Buffer> indirect_buffer;
			UINT offset_for_args = 0;

			buffer_view() = default;
			~buffer_view() {}
		};

		buffer_view vertex_buffer_view;	//	頂点バッファビュー
		material_datas material_data;	//	マテリアル情報
		D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	};

	std::vector<sprite_trail> sprite_trail_draw_datas;
	std::vector<std::size_t> sprite_trail_color_datas;
	std::vector<std::size_t> sprite_trail_shadow_datas;

	void clear()
	{
		sprite_trail_draw_datas.clear();
		sprite_trail_color_datas.clear();
		sprite_trail_shadow_datas.clear();
	}
};

//	デカール情報
//	@remaidner	時間が無いのでレンダラー側にモデルを持たせている
struct decal_datas
{
	math::float3 scale = {1, 1, 1};
	math::rotatorf rotation = {0, 0, 0};
	math::float3 position = {0, 0, 0};
	collision::bounding_box box;
	material_datas material;
};
struct decal_container
{
	std::vector<decal_datas> decal_datas;
	void clear()
	{
		decal_datas.clear();
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
	};
	buffer_datas line_buffer_datas[magic_enum::enum_count<line_buffer_id>()];
};
#endif // defined(USE_IMGUI)

//	描画データコンテキスト
struct render_data_context
{
	math::vector3d render_origin;	//	描画データ収集時の描画原点

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
	ID3D11DeviceContext* device_context;
	std::shared_ptr<render_data_context> render_data;

	//	シーン情報
	common_pipeline_states::scene_constants scene_constant;
	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;

	//	IBL関係
	texture_ptr diffuse_ibl_texture;
	texture_ptr specluar_ibl_texture;
	texture_ptr lut_ggx_texture;

	//	シーンテクスチャ
	texture_ptr scene_color_texture;
	texture_ptr scene_depth_texture;
	texture_ptr g_buffer_textures[magic_enum::enum_count<common_pipeline_states::gbuffer_id>()];
	texture_ptr work_texture[8];

	//	遅延ライティング処理用情報
	common_pipeline_states::deferred_light_constants deferred_light_constant;
	common_pipeline_states::deferred_light_probe_constants deferred_light_probe_constant;

	//	シャドウマップ用情報
	bool is_shadow_rendering = false;	//	影生成描画処理中判定
	common_pipeline_states::shadow_map_constants shadow_map_constant;
	texture_ptr shadow_map_texture;

	//	強制設定フラグ
	bool is_force_material_wire_draw = false;
	bool is_force_material_both_draw = false;

	//	スクリーン空間反射用情報
	common_pipeline_states::screen_space_reflection_constants screen_space_reflection_constant;

	//	クラスタードライティング
	std::shared_ptr<clustered_lighting_system> clustered_lighting;

public:
	//	ラスタライザーステート設定
	void set_rasterizer_state(ID3D11RasterizerState* state) const;

	//	深度ステンシルステート設定
	void set_depth_stencil_state(ID3D11DepthStencilState* state, UINT stencil_ref) const;

	//	ブレンドステート設定
	void set_blend_state(ID3D11BlendState* state) const;

	//	図形設定
	void set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY topology) const;

	//	頂点バッファ設定
	void set_vertex_buffers(UINT start_slot, UINT num_buffers, ID3D11Buffer* const* buffers, const UINT* strides, const UINT* offsets) const;

	//	インデックスバッファ設定
	void set_index_buffer(ID3D11Buffer* buffer, DXGI_FORMAT format, UINT offset) const;

	//	頂点ストリーム設定
	void set_vertex_streams(UINT num_streams, ID3D11ShaderResourceView* const* streams) const;

	//	設定していたシェーダーを全て解除する
	void clear_shader_slots() const;

	//	設定されている頂点バッファを外す
	void clear_vertex_buffer() const;

	//	設定されているインデックスバッファを外す
	void clear_index_buffer() const;

	//	描画ターゲットのクリア
	void clear_render_target_slots() const;

	//	CBVのクリア設定
	void clear_constant_buffers(int start_slot = 0, int num = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) const;

	//	設定していたSRVを全て解除する
	void clear_shader_resorce_views(int start_slot = 0, int num = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) const;

	//	設定していたUAVを全て解除する
	//	FEATURELEVELが11.1ならnumはD3D11_1_UAV_SLOT_COUNTになる
	void clear_unordered_access_views(int start_slot = 0, int num = 8) const;

	//	設定していたSamplerを全て解除する
	void clear_sampler(int start_slot = 0, int num = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) const;

	//	デバッグイベント開始
	void debug_begin_event(const wchar_t* event_name);

	//	デバッグイベント終了
	void debug_end_event();

private:
	//	キャッシュ情報
	mutable ID3D11RasterizerState* chache_rasterizer_state = nullptr;
	mutable ID3D11DepthStencilState* chache_depth_stencil_state = nullptr;
	mutable ID3D11BlendState* chache_blend_state = nullptr;
	mutable UINT chache_stencil_ref = 0xFFFFFFFF;
	mutable D3D_PRIMITIVE_TOPOLOGY chache_primitive_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	mutable UINT chache_vertex_buffer_start_slot = 0xFFFFFFFF;
	mutable UINT chache_vertex_buffer_count = 0;
	mutable ID3D11Buffer* chache_vertex_buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	mutable UINT chache_vertex_buffer_strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	mutable UINT chache_vertex_buffer_offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	mutable ID3D11Buffer* chache_index_buffer = nullptr;
	mutable DXGI_FORMAT chache_index_buffer_format = DXGI_FORMAT_UNKNOWN;
	mutable UINT chache_index_buffer_offset = 0xFFFFFFFF;
};
