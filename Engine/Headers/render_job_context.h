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
		Microsoft::WRL::ComPtr<ID3D11Buffer> copy_source;	//	@tool 設定されていれば、このバッファの先頭から size_in_bytes 分を buffer へコピーする
		UINT size_in_bytes = 0;
	};
	std::vector<command> commands;

	//	データを複製して積む
	void add_copy(const Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, bool use_box, UINT offset_in_bytes, const void* data, UINT size_in_bytes);

	//	データの所有権を移して積む
	void add_owned(const Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, bool use_box, UINT offset_in_bytes, std::vector<uint8_t>&& data);

	//	このフレームの描画データ内のデータを参照して積む
	void add_reference(const Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, bool use_box, UINT offset_in_bytes, const void* data, UINT size_in_bytes);

	//	@tool バッファ間のコピーを積む（大きくしたバッファへ古い中身を移す。前に積んだ転送より後に、GPU の順番どおりに行われる）
	void add_buffer_copy(const Microsoft::WRL::ComPtr<ID3D11Buffer>& destination, const Microsoft::WRL::ComPtr<ID3D11Buffer>& source, UINT size_in_bytes);

	//	GPU転送処理
	void upload(const render_command_recorder& recorder) const;

	//	空判定
	bool empty() const { return commands.empty(); }
};

//	材質
class material_render_proxy;

//	メッシュ描画情報コンテナ
struct mesh_container
{
	//	カプセルシャドウ情報
	struct capsule_shadow_item
	{
		collision::bounding_capsule capsule;
	};

	std::vector<mesh_draw_item> draw_items;
	std::vector<capsule_shadow_item> capsule_shadows;
	std::vector<std::size_t> color_items;
	std::vector<std::size_t> shadow_items;

	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_header_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> primitive_header_srv;
	UINT primitive_header_capacity = 0;
	Microsoft::WRL::ComPtr<ID3D11Buffer> bone_matrix_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bone_matrix_srv;
	UINT bone_matrix_capacity = 0;
	std::vector<common_pipeline_states::primitive_header> primitive_header_scratch;
	std::vector<math::float4x4> bone_matrix_scratch;
	std::vector<std::shared_ptr<const void>> keep_alive_objects;
	//	@tool メッシュレットで読む頂点属性とメッシュレットのバッファ（抽出時のモデル用プールの物。描画側はプールを直接引かない）
	//	@tool 並びは scene_renderer_commons::capture_meshlet_attribute_srvs の物
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> meshlet_attribute_srvs[16];

	gpu_upload_list transform_uploads;

	void build_transform_buffers();
	void clear();
};

//	スプライト情報コンテナ
struct sprite_render_proxy;
struct sprite_draw_item
{
	int sort_key;
	int sort_order;
	std::shared_ptr<const sprite_render_proxy> sprite;
};
struct sprite_container
{
	std::vector<sprite_draw_item> draw_items;
};

//	パーティクル描画単位
struct particle_draw_item
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
	std::shared_ptr<const material_render_proxy> material;	//	マテリアル情報
	D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

//	エフェクト情報コンテナ
struct effect_container
{
	std::vector<particle_draw_item> particle_draw_items;
	std::vector<std::size_t> particle_color_items;
	std::vector<std::size_t> particle_shadow_items;

	void clear()
	{
		particle_draw_items.clear();
		particle_color_items.clear();
		particle_shadow_items.clear();
	}
};

//	デカール情報
//	@remaidner	時間が無いのでレンダラー側にモデルを持たせている
struct decal_draw_item
{
	math::float3 scale = {1, 1, 1};
	math::rotatorf rotation = {0, 0, 0};
	math::float3 position = {0, 0, 0};
	collision::bounding_box box;
	std::shared_ptr<const material_render_proxy> material;
};
struct decal_container
{
	std::vector<decal_draw_item> draw_items;
	void clear()
	{
		draw_items.clear();
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
	mesh_draw_item line_draw_items[static_cast<int>(line_buffer_id::max)];
	std::vector<mesh_draw_item::sub_mesh> line_meshes[static_cast<int>(line_buffer_id::max)];
	std::unordered_map<semantic, buffer_view> line_vertex_buffer_views[static_cast<int>(line_buffer_id::max)];
};
#endif // defined(USE_IMGUI)

//	描画データコンテキスト
struct view_render_data
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
//	@tool ジョブごとに作る。コマンドはデバイスコンテキストごとの recorder に積む
struct render_job_context
{
	render_command_recorder& recorder;	//	描画コマンドレコーダー
	std::shared_ptr<view_render_data> render_data;	//	描画データ

	//	シーン情報
	common_pipeline_states::scene_constants scene_constant;
	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;

	//	クラスタードライティング
	std::shared_ptr<clustered_lighting_system> clustered_lighting;

public:
	explicit render_job_context(render_command_recorder& recorder) : recorder(recorder) {}

	//	バインドテーブルにフレーム・視点層のリソースを登録
	void register_frame_bind_table();
};
