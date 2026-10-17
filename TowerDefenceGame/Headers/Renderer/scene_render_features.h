#pragma once

#include "Renderer/frame_graph.h"
#include "Renderer/scene_render_targets.h"
#include "Renderer/scene_renderer_common.h"
#include "geometric_primitive.h"
#include "graphics/fast_fourier_transform.h"

namespace scene_rendering
{
	//	視点のフレーム描画情報
	struct scene_frame
	{
		renderer_context context;
		std::shared_ptr<scene_render_targets> targets;
		scene_renderer_commons::scene_texture_handles textures;
	};

	//	@tool G-Buffer
	struct gbuffer_resources
	{
		graphics_pipeline_state_ptr skybox_pipeline_states;
	};

	//	@tool G-Buffer デカール
	struct decal_resources
	{
		std::shared_ptr<geometric_cube> cube;
		Microsoft::WRL::ComPtr<ID3D11Buffer> decal_primitive_primitive_buffer;
		graphics_pipeline_state_ptr stencil_write_pipeline_state;
	};

	//	@tool ディファードライティング
	struct deferred_lighting_resources
	{
		static constexpr UINT directional_shadow_map_count = 6;
		std::shared_ptr<material> deferred_lighting_material;
	};

	//	@tool ポストプロセスマテリアル
	struct post_process_resources
	{
		graphics_pipeline_state_ptr write_back;
	};

	//	@tool スクリーンスペースリフレクション
	struct screen_space_reflection_resources
	{
		graphics_pipeline_state_ptr ssr_uv_pipeline_state;
		graphics_pipeline_state_ptr ssr_color_pipeline_state;
		graphics_pipeline_state_ptr ssr_composite_pipeline_state;
		graphics_pipeline_state_ptr write_back;
		std::shared_ptr<compute_shader> denoise;
		std::shared_ptr<compute_shader> temporal_filter;
		Microsoft::WRL::ComPtr<ID3D11Buffer> denoise_constant_buffer;
	};

	//	@tool 被写界深度
	struct depth_of_field_resources
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
		std::shared_ptr<compute_shader> dof_filter_setup;
		std::shared_ptr<compute_shader> dof_vertical_integrate;
		std::shared_ptr<compute_shader> dof_apply;
	};

	//	@tool モーションブラー
	struct motion_blur_resources
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
		std::shared_ptr<compute_shader> reconstract_velocity;
		std::shared_ptr<compute_shader> motion_blur;
	};

	//	@tool ブルームとトーンマッピング
	struct bloom_resources
	{
		//	平均輝度算出
		std::shared_ptr<compute_shader> correct_luminance;

		//	汎用シェーダー
		std::shared_ptr<compute_shader> copy_texture;
		std::shared_ptr<compute_shader> scene_additive;

		//	FFT関係
		Microsoft::WRL::ComPtr<ID3D11Buffer> fft_gaussian_constant_buffer;
		std::shared_ptr<compute_shader> fft_gaussian_filter;
		std::shared_ptr<compute_shader> fft_multiply;

		//	輝度抽出
		Microsoft::WRL::ComPtr<ID3D11Buffer> luminance_extract_constant_buffer;
		std::shared_ptr<compute_shader> luminance_extract;
		texture_ptr lens_ghost_luminance_map;

		//	ゴースト用
		Microsoft::WRL::ComPtr<ID3D11Buffer> lens_ghost_constant_buffer;
		std::shared_ptr<compute_shader> generate_ghost;

		//	トーンマッピング用
		Microsoft::WRL::ComPtr<ID3D11Buffer> tone_mapping_constant_buffer;
		std::shared_ptr<compute_shader> tone_mapping;
	};

	//	@tool ブルームの記録処理だけが書き換える物（FFT の作業領域と、変換済みカーネルの記録）
	//	@tool ブルームのパスは immediate context で追加順に実行されるので、全視点で共有しても同時には使われない
	struct bloom_fft_state
	{
		std::unique_ptr<fast_fourier_transform> scene_fft;
		std::unique_ptr<fast_fourier_transform> bloom_kernel_fft;
		texture_ptr bloom_image_kernel_texture;
		std::unique_ptr<fast_fourier_transform> lens_ghost_kernel_fft;
		texture_ptr lens_ghost_image_kernel_texture;
	};

	//	@tool アンチエイリアス
	struct anti_alias_resources
	{
		graphics_pipeline_state_ptr write_back;
		graphics_pipeline_state_ptr fxaa;
		graphics_pipeline_state_ptr smaa_edge_detection;
		graphics_pipeline_state_ptr smaa_blending_weight_calculation;
		graphics_pipeline_state_ptr smaa_neighborhood_blending;
	};

	//	@tool ファイナルパス
	struct final_pass_resources
	{
		graphics_pipeline_state_ptr convert_linear_to_srgb_pipeline_states;
	};

#if defined(USE_IMGUI)
	//	@tool デバッグ描画
	struct debug_draw_resources
	{
		graphics_pipeline_state_ptr debug_draw_line_pipeline_states[static_cast<int>(debug_draw_primitive_container::line_buffer_id::max)];
	};
#endif	//	defined(USE_IMGUI)

	//	@tool UI
	struct ui_resources
	{
		std::unordered_map<semantic, buffer_view> vertex_buffer_views;
		buffer_view index_buffer_view;
		Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_constant_buffer;
	};

	//	描画機能毎の共有リソース
	class scene_render_features
	{
	public:
		scene_render_features();

		std::shared_ptr<const gbuffer_resources> gbuffer;
		std::shared_ptr<const decal_resources> decal;
		std::shared_ptr<const deferred_lighting_resources> deferred_lighting;
		std::shared_ptr<const post_process_resources> post_process;
		std::shared_ptr<const screen_space_reflection_resources> screen_space_reflection;
		std::shared_ptr<const depth_of_field_resources> depth_of_field;
		std::shared_ptr<const motion_blur_resources> motion_blur;
		std::shared_ptr<const bloom_resources> bloom;
		std::shared_ptr<bloom_fft_state> bloom_fft;
		std::shared_ptr<const anti_alias_resources> anti_alias;
		std::shared_ptr<const final_pass_resources> final_pass;
#if defined(USE_IMGUI)
		std::shared_ptr<const debug_draw_resources> debug_draw;
#endif	//	defined(USE_IMGUI)
		std::shared_ptr<const ui_resources> ui;
	};

	//	視点1つ分のシーンのパスを、実行する順に足す
	void add_scene_passes(frame_graph& graph, scene_frame& frame, const scene_render_features& features);

	//	@tool UI のパスを足す（メイン画面だけに描く）
	void add_ui_pass(frame_graph& graph, const renderer_context& context, const std::shared_ptr<const scene_render_targets>& targets, const scene_render_features& features);

	//	@tool 共有資源の生成（scene_render_features のコンストラクターから呼ぶ）
	void initialize_deferred_features(scene_render_features& features);
	void initialize_post_process_features(scene_render_features& features);
	void initialize_output_features(scene_render_features& features);
	std::shared_ptr<const ui_resources> create_ui_resources();

	//	@tool 各機能のパス（add_scene_passes から順に呼ぶ）
	void add_depth_prepass(frame_graph& graph, const scene_frame& frame);
	void add_occlusion_culling_pass(frame_graph& graph, const scene_frame& frame);
	void add_gbuffer_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const gbuffer_resources>& resources);
	void add_gbuffer_decal_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const decal_resources>& resources);
	void add_deferred_lighting_passes(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const deferred_lighting_resources>& resources);
	void add_screen_space_reflection_pass(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const screen_space_reflection_resources>& resources);
	void add_forward_pass(frame_graph& graph, const scene_frame& frame, material::translucency_paths path, std::string_view pass_name);
	void add_post_process_material_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const post_process_resources>& resources, material::blendable_locations location);
	void add_depth_of_field_pass(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const depth_of_field_resources>& resources);
	void add_motion_blur_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const motion_blur_resources>& resources);
	void add_bloom_and_tone_mapping_passes(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const bloom_resources>& resources, const std::shared_ptr<bloom_fft_state>& fft);
	void add_anti_alias_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const anti_alias_resources>& resources);
	void add_final_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const final_pass_resources>& resources);
#if defined(USE_IMGUI)
	void add_debug_draw_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const debug_draw_resources>& resources);
#endif	//	defined(USE_IMGUI)
}	//	namespace scene_rendering
