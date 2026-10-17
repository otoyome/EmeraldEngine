#pragma once

#include "Renderer/frame_graph.h"
#include "render_pass.h"
#include "scene_render_targets.h"
#include "geometric_primitive.h"

#include "collision/frustum_culling.h"
#include "graphics/meshlet_culling_system.h"
#include "graphics/hiz_buffer.h"

namespace scene_renderer_commons
{
	//	シーン用のテクスチャをレンダーグラフに取り込んだ番号（パスの読み書きの宣言に使う）
	struct scene_texture_handles
	{
		frame_graph::texture_handle scene_color;
		frame_graph::texture_handle scene_depth;
		frame_graph::texture_handle shadow_map;
		frame_graph::texture_handle g_buffers[static_cast<UINT>(common_pipeline_states::gbuffer_id::max)];
		frame_graph::texture_handle ssr_accumlate[2];
		frame_graph::texture_handle ssr_counter[2];
		frame_graph::texture_handle output;

		frame_graph::texture_handle g_buffer(common_pipeline_states::gbuffer_id id) const { return g_buffers[static_cast<UINT>(id)]; }

		void append_g_buffers(std::vector<frame_graph::texture_handle>& handles) const;
	};

	//	シーン用のテクスチャを取り込む
	scene_texture_handles import_scene_textures(frame_graph& graph, const scene_render_targets& targets);

	//	@tool シーンと同じ大きさ（divisor 分の1）の作業用テクスチャの形式（一時テクスチャとして要求する）
	D3D11_TEXTURE2D_DESC make_work_texture_desc(const scene_render_targets& targets, DXGI_FORMAT format, UINT divisor = 1);

	//	@tool 描画コマンド1件
	//	@tool パスを追加する時（メインスレッド）にパイプライン・マテリアルを解決して作り、描画ジョブは順に実行するだけにする
	struct mesh_draw_command
	{
		const graphics_pipeline_state* pipeline = nullptr;
		const material_render_proxy* material = nullptr;
		UINT data_index = 0;		//	モデルは mesh_container::buffer_datas、パーティクルは effect_container::sprite_trail_draw_datas の番号
		float mesh_length = 0.0f;	//	LOD 選択用の距離
		bool fallback = false;		//	メッシュレットの間接描画でまとめられない（モーフ持ち）
	};
	using mesh_draw_command_list = std::vector<mesh_draw_command>;
	using mesh_draw_command_list_ptr = std::shared_ptr<const mesh_draw_command_list>;

	//	@tool 影の描画コマンド（マテリアル1つ分のメッシュレットをまとめて描く）
	struct shadow_draw_batch
	{
		const graphics_pipeline_state* pipeline = nullptr;
		const material_render_proxy* material = nullptr;
		std::vector<common_pipeline_states::meshlet_instance_record> instance_records;
	};
	using shadow_draw_batch_list = std::vector<shadow_draw_batch>;
	using shadow_draw_batch_list_ptr = std::shared_ptr<const shadow_draw_batch_list>;

	//	ビュー空間の深度をカメラの射影深度に変換する(MATH_REVERSE_DEPTHの設定に従う)
	float convert_view_depth_to_projection_depth(float view_depth, float near_clip, float far_clip);

	void draw(const graphics_context& context,
			  graphics_pipeline_state_ptr pipeline_state,
			  UINT vertex_count,
			  D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
			  UINT vertex_offset = 0);

	void draw_indexed(const graphics_context& context,
					  float mesh_length,
					  const buffer_datas& buffer_data,
					  UINT primitive_index,
					  graphics_pipeline_state_ptr pipeline_state,
					  const collision::frustum_culling* meshlet_frustum_culling = nullptr);

	void draw_geometric_primitive(const graphics_context& context,
					  const geometric_primitive& primitive);

	//	@tool 描画コマンドの作成（メインスレッド）
	mesh_draw_command_list_ptr build_model_draw_commands(const render_data_context& render_data,
					  const std::vector<std::size_t>& mesh_indices,
					  const std::vector<float>& mesh_lengths,
					  material::surface_paths surface_path);
	mesh_draw_command_list_ptr build_model_indirect_draw_commands(const render_data_context& render_data,
					  const std::vector<std::size_t>& mesh_indices,
					  const std::vector<float>& mesh_lengths,
					  material::surface_paths surface_path);
	mesh_draw_command_list_ptr build_particle_draw_commands(const render_data_context& render_data,
					  const std::vector<std::size_t>& draw_indices,
					  material::surface_paths surface_path);

	//	@tool 描画コマンドの実行（描画ジョブ）
	void execute_model_draw_commands(graphics_context& context,
					  const mesh_draw_command_list& commands,
					  const collision::frustum_culling* meshlet_frustum_culling = nullptr);
	void execute_model_indirect_draw_commands(graphics_context& context,
					  const mesh_draw_command_list& commands,
					  meshlet_culling_system& meshlet_culling);
	void execute_particle_draw_commands(graphics_context& context,
					  const mesh_draw_command_list& commands);

	//	@tool 影の描画（マテリアル単位でまとめたメッシュレットの一覧）
	//	@tool ライトの錐台・球によるカリングとまとめはパスを追加する時（メインスレッド）に行い、描画ジョブは実行するだけにする
	shadow_draw_batch_list_ptr build_shadow_draw_batches(const render_data_context& render_data,
					  const std::vector<std::size_t>& mesh_indices,
					  const collision::frustum_culling& frustum_culling,
					  material::surface_paths surface_path);
	shadow_draw_batch_list_ptr build_shadow_draw_batches(const render_data_context& render_data,
					  const std::vector<std::size_t>& mesh_indices,
					  const math::float3& sphere_center,
					  float sphere_radius,
					  material::surface_paths surface_path);
	void execute_shadow_draw_batches(graphics_context& context, const shadow_draw_batch_list& batches);
}	//	namespace scene_renderer_commons
