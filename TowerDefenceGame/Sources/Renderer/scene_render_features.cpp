#include "Renderer/scene_render_features.h"

#include "Editor/detail_profiler.h"

using namespace scene_rendering;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
scene_render_features::scene_render_features()
{
	initialize_deferred_features(*this);
	initialize_post_process_features(*this);
	initialize_output_features(*this);
	ui = create_ui_resources();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	フォワードレンダリング（半透明）
void scene_rendering::add_forward_pass(frame_graph& graph, const scene_frame& frame, material::translucency_paths path, std::string_view pass_name)
{
	const int forward_index = static_cast<int>(path);
	const renderer_context& renderer_context = frame.context;
	if (renderer_context.render_mesh_indices->forward[forward_index].empty() &&
		renderer_context.render_particle_indices->forward[forward_index].empty())
		return;

	struct pass_data
	{
		scene_renderer_commons::mesh_draw_command_list_ptr model_commands;
		scene_renderer_commons::mesh_draw_command_list_ptr particle_commands;
		D3D11_VIEWPORT viewport{};
		UINT render_pass = 0;
	};
	graph.add_pass<pass_data>(pass_name, frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		//	カラーパス生成（深度は読み、ステンシルはクリアする）
		frame_graph::color_attachment color;
		color.texture = frame.textures.scene_color;
		color.clear = false;
		frame_graph::depth_attachment depth;
		depth.texture = frame.textures.scene_depth;
		depth.format = common_pipeline_states::scene_depth_stencil_view_format;
		depth.clear_depth = false;
		depth.clear_stencil = true;
		data.render_pass = builder.add_render_pass({ color }, depth);
		data.viewport = frame.targets->get_scene_viewport();

		//	@tool 描画コマンドはパスを追加する時に作っておく（描画ジョブは実行するだけ）
		const render_data_context& frame_data = *renderer_context.render_data;
		data.model_commands = scene_renderer_commons::build_model_draw_commands(frame_data, renderer_context.render_mesh_indices->forward[forward_index], renderer_context.render_mesh_indices->lengths, material::surface_paths::forward_pass);
		data.particle_commands = scene_renderer_commons::build_particle_draw_commands(frame_data, renderer_context.render_particle_indices->forward[forward_index], material::surface_paths::forward_pass);
	},
	[](const pass_data& data, const frame_graph::resources& resources, graphics_context& context)
	{
		const render_pass& rendering_pass = resources.get_render_pass(data.render_pass);

		//	描画先設定
		rendering_pass.activate(context.recorder.get_device_context());

		//	ビューポートの設定
		context.recorder.get_device_context()->RSSetViewports(1, &data.viewport);

		//	モデル類描画
		scene_renderer_commons::execute_model_draw_commands(context, *data.model_commands);
		scene_renderer_commons::execute_particle_draw_commands(context, *data.particle_commands);

		rendering_pass.deactivate(context.recorder.get_device_context());
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	@tool 視点1つ分のシーンのパスを、実行する順に足す
void scene_rendering::add_scene_passes(frame_graph& graph, scene_frame& frame, const scene_render_features& features)
{
	auto add = [&graph](std::string_view owner_name, auto&& function)
	{
		DETAIL_PROFILE_CPU_SCOPE_CATEGORY(renderer_issue, owner_name);
		frame_graph::owner_scope owner(graph, owner_name);
		function();
	};

	//	深度プリパス・オクルージョンカリング
	add("depth_prepass", [&] { add_depth_prepass(graph, frame); });
	add("occlusion_culling", [&] { add_occlusion_culling_pass(graph, frame); });

	//	G-Buffer生成・ライティング
	add("gbuffer", [&] { add_gbuffer_pass(graph, frame, features.gbuffer); });
	add("gbuffer_decal", [&] { add_gbuffer_decal_pass(graph, frame, features.decal); });
	add("deferred_lighting", [&] { add_deferred_lighting_passes(graph, frame, features.deferred_lighting); });
	add("screen_space_reflection", [&] { add_screen_space_reflection_pass(graph, frame, features.screen_space_reflection); });

	//	被写界深度
	add("before_dof_forward", [&] { add_forward_pass(graph, frame, material::translucency_paths::before_dof, "forward_pass : before_dof"); });
	add("before_dof_post_process", [&] { add_post_process_material_pass(graph, frame, features.post_process, material::blendable_locations::before_dof); });
	add("depth_of_field", [&] { add_depth_of_field_pass(graph, frame, features.depth_of_field); });
	add("after_dof_post_process", [&] { add_post_process_material_pass(graph, frame, features.post_process, material::blendable_locations::after_dof); });
	add("after_dof_forward", [&] { add_forward_pass(graph, frame, material::translucency_paths::after_dof, "forward_pass : after_dof"); });
	add("after_translucency_post_process", [&] { add_post_process_material_pass(graph, frame, features.post_process, material::blendable_locations::after_dof_translucency); });

	//	モーションブラー
	add("motion_blur", [&] { add_motion_blur_pass(graph, frame, features.motion_blur); });
	add("after_motion_blur_forward", [&] { add_forward_pass(graph, frame, material::translucency_paths::after_motion_blur, "forward_pass: after_motion_blur"); });

	//	トーンマッピング
	add("before_bloom_post_process", [&] { add_post_process_material_pass(graph, frame, features.post_process, material::blendable_locations::before_bloom); });
	add("bloom_and_tone_mapping", [&] { add_bloom_and_tone_mapping_passes(graph, frame, features.bloom, features.bloom_fft); });
	add("after_tone_mapping_post_process", [&] { add_post_process_material_pass(graph, frame, features.post_process, material::blendable_locations::after_tone_mapping); });

	//	最終パス
	add("anti_alias", [&] { add_anti_alias_pass(graph, frame, features.anti_alias); });
	add("final_pass", [&] { add_final_pass(graph, frame, features.final_pass); });

#if defined(USE_IMGUI)
	//	デバッグ描画
	add("debug_draw", [&] { add_debug_draw_pass(graph, frame, features.debug_draw); });
#endif // defined(USE_IMGUI)
}
