#include "Renderer/scene_render_features.h"

#include "model_instance.h"

#include "custom_pipeline_state.h"
#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "PipelineState/post_process_pipeline_state.h"
#include "PipelineState/anti_alias_pipeline_state.h"

#include "System/engine_config_data.h"

using namespace scene_rendering;

namespace
{
	void draw(const render_job_context& context,
			  graphics_pipeline_state_ptr pipeline_state,
			  UINT vertex_count,
			  D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
			  UINT vertex_offset = 0)
	{
		mesh_draw_item draw_item;
		draw_item.vertex_count = vertex_count;
		context.recorder.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		context.recorder.set_primitive_topology(primitive_topology);
		context.recorder.draw(vertex_count, vertex_offset);
	}

	//	@tool シーンへ書き戻す描画先（全体を上書きするのでクリアしてから描く）
	frame_graph::color_attachment scene_color_attachment(const scene_frame& frame)
	{
		frame_graph::color_attachment color;
		color.texture = frame.textures.scene_color;
		color.clear = true;
		return color;
	}

	//	@tool 作業用の描画先（クリアしてから描く）
	frame_graph::color_attachment work_attachment(frame_graph::texture_handle texture, const std::array<FLOAT, 4>& clear_value = { 0.0f, 0.0f, 0.0f, 0.0f })
	{
		frame_graph::color_attachment color;
		color.texture = texture;
		color.clear = true;
		color.clear_value = clear_value;
		return color;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region PostprocessMaterial
void scene_rendering::add_post_process_material_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const post_process_resources>& resources, material::blendable_locations blendable_location)
{
	//	ポストプロセスボリュームが無ければ適応しない
	post_process_volume_render_proxy_ptr inner_volume = frame.context.render_data->post_process_volume_container.selected_volume;
	if (!inner_volume)
		return;

	//	@tool 描画するマテリアル（パイプラインステートが無い物は描かない）
	using material_ptr = std::decay_t<decltype(*std::begin(inner_volume->get_materials()))>;
	std::vector<material_ptr> materials;
	for (auto& material : inner_volume->get_materials())
	{
		if (material->get_domain() != material::domains::post_process ||
			material->get_blendable_location() != blendable_location)
			continue;
		if (!material->get_pipeline_state())
			continue;
		materials.emplace_back(material);
	}
	if (materials.empty())
		return;

	std::string render_pass_name = std::string(magic_enum::enum_name(blendable_location));
	render_pass_name += "_post_process_material_pass";

	//	@tool 作業用は一時テクスチャとして要求する（マテリアルごとに交互に描き、最後にシーンへ書き戻す）
	const D3D11_TEXTURE2D_DESC work_desc = scene_renderer_commons::make_work_texture_desc(frame.config, DXGI_FORMAT_R16G16B16A16_FLOAT);
	const frame_graph::texture_handle work_buffers[2] =
	{
		graph.create_texture("post_process_work[0]", work_desc),
		graph.create_texture("post_process_work[1]", work_desc),
	};

	//	@tool マテリアル1つにつき1パス（前の結果を読み、作業用へ描く）
	struct material_pass_data
	{
		material_ptr material;
		frame_graph::texture_handle source;
		frame_graph::texture_handle scene_depth;
		D3D11_VIEWPORT viewport{};
	};
	frame_graph::texture_handle source = frame.textures.scene_color;
	for (std::size_t index = 0; index < materials.size(); ++index)
	{
		const frame_graph::texture_handle destination = work_buffers[(index + 1) % 2];
		graph.add_pass<material_pass_data>(render_pass_name + "[" + std::to_string(index) + "]", frame.context, [&](frame_graph::builder& builder, material_pass_data& data)
		{
			data.source = builder.read(source);
			data.scene_depth = builder.read(frame.textures.scene_depth);
			builder.set_render_targets({ work_attachment(destination) });
			data.material = materials[index];
			data.viewport = frame.config.get_viewport();
		},
		[](const material_pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			auto pipeline_state = data.material->get_pipeline_state();
			if (!pipeline_state)
				return;

			//	ビューポートの設定
			context.recorder.set_viewport(data.viewport);

			//	シーン描画
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.source));
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_depth_map_hash_key, resources.get_texture(data.scene_depth));

			context.recorder.set_pipeline(*pipeline_state);
			context.recorder.bind_draw(*pipeline_state, *data.material, nullptr);
			draw(context, pipeline_state, 4);
			context.recorder.end_pipeline();
		});
		source = destination;
	}

	//	書き戻し
	struct write_back_pass_data
	{
		std::shared_ptr<const post_process_resources> resources;
		frame_graph::texture_handle source;
		D3D11_VIEWPORT viewport{};
	};
	graph.add_pass<write_back_pass_data>(render_pass_name + "_write_back", frame.context, [&](frame_graph::builder& builder, write_back_pass_data& data)
	{
		data.source = builder.read(source);
		builder.set_render_targets({ scene_color_attachment(frame) });
		data.resources = resources;
		data.viewport = frame.config.get_viewport();
	},
	[](const write_back_pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		//	ビューポートの設定
		context.recorder.set_viewport(data.viewport);

		//	シーン描画
		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.source));

		context.recorder.set_pipeline(*data.resources->write_back);
		draw(context, data.resources->write_back, 4);
		context.recorder.end_pipeline();
	});
}
#pragma endregion	//	PostprocessMaterial

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region SSR

//	半解像度でSSRを生成する
//	@remainder	G-Bufferを縮小して生成し、
//				バイラテラルアップサンプリングするようにしないと縁の部分がおかしくなるがやってない。
#define	USE_SSR_HALF_BUFFER

namespace ssr
{
	struct denoise_constants
	{
		math::uint2	texture_size;
		float				reset_flag;
		float				iteration;

		float				denoise_velocity_max;
		float				denoise_color_sigma;
		float				denoise_normal_sigma;
		float				denoise_depth_sigma;

		float				denoise_temporal_blend_factor;
		float				denoise_counter_max;
		math::uint2	denoise_dummy;
	};
	constexpr math::uint2 denoise_threads = { 16, 16 };
	constexpr UINT denoise_cbv_index = 3;

	//	@tool 前のフレームから引き継ぐ蓄積バッファの名前（視点の状態に持つ）
	constexpr view_history_key accumlate_history_key = system_utility::generate_hash("screen_space_reflection_accumlate");
	constexpr view_history_key counter_history_key = system_utility::generate_hash("screen_space_reflection_counter");

	//	@tool パスのデータ（反射UV・反射色・デノイズ・合成・書き戻しの各パスが同じ物を持つ）
	struct pass_data
	{
		std::shared_ptr<const screen_space_reflection_resources> resources;
		post_process_volume_render_proxy_ptr inner_volume;
		bool reset_history = false;	//	@tool 前のフレームの蓄積が無い（最初のフレーム・大きさの変更後）ので、クリアしてから読む
		scene_renderer_commons::scene_texture_handles textures;
		frame_graph::texture_handle previous_accumlate;
		frame_graph::texture_handle previous_counter;
		frame_graph::texture_handle accumlate;
		frame_graph::texture_handle counter;
		frame_graph::texture_handle uv_buffer;
		frame_graph::texture_handle work_buffers[2];
		D3D11_VIEWPORT scene_color_viewport{};
	};

	//	@tool SSR の定数（固定のパイプラインステートが参照するので、描くパスごとにパス層へ登録する）
	void set_constants(render_job_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		auto& ssr_data = data.inner_volume->get_screen_space_reflection_constant();
		const D3D11_VIEWPORT uv_viewport = resources.get_viewport(data.uv_buffer);

		common_pipeline_states::screen_space_reflection_constants ssr_constant{};
		ssr_constant.ray_resolution = ssr_data.ray_resolution;
		ssr_constant.ray_max_distance = ssr_data.ray_max_distance;
		ssr_constant.ray_substep_iteration_max = ssr_data.ray_substep_iteration_max;
		ssr_constant.ray_flags = ssr_data.ray_flags;
		ssr_constant.ray_tickness = ssr_data.ray_tickness;
		ssr_constant.viewport_size.x = uv_viewport.Width;
		ssr_constant.viewport_size.y = uv_viewport.Height;
		ssr_constant.viewport_size.z = 1.0f / uv_viewport.Width;
		ssr_constant.viewport_size.w = 1.0f / uv_viewport.Height;
		context.recorder.get_bind_table().set_constants(resource_view_bind_layer::pass, common_pipeline_states::screen_space_reflection_constant_buffer_hash_key, ssr_constant);
	}

	//	@tool G-Buffer をパス層へ設定する（基本色の代わりにシーンの色を使う）
	void bind_scene_textures(render_job_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[index], resources.get_texture(data.textures.g_buffers[index]));
		}
		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[static_cast<UINT>(common_pipeline_states::gbuffer_id::base_color)], resources.get_texture(data.textures.scene_color));
	}

	//	反射UV情報を算出
	void render_uv(render_job_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		//	@tool 前のパスの割り当てを外しておく（描画先はグラフが設定済みなので外さない）
		context.recorder.clear_constant_buffers();
		context.recorder.clear_shader_slots();

		set_constants(context, data, resources);
		context.recorder.set_viewport(resources.get_viewport(data.uv_buffer));

		//	SRV設定
		bind_scene_textures(context, data, resources);

		context.recorder.set_pipeline(*data.resources->ssr_uv_pipeline_state);
		draw(context, data.resources->ssr_uv_pipeline_state, 4);
		context.recorder.end_pipeline();
	}

	//	反射色情報を算出
	void render_color(render_job_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		set_constants(context, data, resources);
		context.recorder.set_viewport(resources.get_viewport(data.work_buffers[1]));

		//	SRV設定
		bind_scene_textures(context, data, resources);
		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[0], resources.get_texture(data.uv_buffer));

		context.recorder.set_pipeline(*data.resources->ssr_color_pipeline_state);
		draw(context, data.resources->ssr_color_pipeline_state, 4);
		context.recorder.end_pipeline();
	}

	void render_denoise(render_job_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		auto& ssr_data = data.inner_volume->get_screen_space_reflection_constant();
		const D3D11_VIEWPORT work_viewport = resources.get_viewport(data.work_buffers[0]);
		const texture_ptr work_textures[2] = { resources.get_texture(data.work_buffers[0]), resources.get_texture(data.work_buffers[1]) };
		const texture_ptr normal_depth_texture = resources.get_texture(data.textures.g_buffer(common_pipeline_states::gbuffer_id::normal_depth));
		const texture_ptr velocity_texture = resources.get_texture(data.textures.g_buffer(common_pipeline_states::gbuffer_id::velocity));
		//	@tool 蓄積バッファは前のフレームの物を読み、今のフレームの物へ書く（次のフレームへ引き継ぐ）
		const texture_ptr previous_accumlate_texture = resources.get_texture(data.previous_accumlate);
		const texture_ptr previous_counter_texture = resources.get_texture(data.previous_counter);
		const texture_ptr accumlate_texture = resources.get_texture(data.accumlate);
		const texture_ptr counter_texture = resources.get_texture(data.counter);

		context.recorder.clear_constant_buffers();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();

		//	@tool 前のフレームの蓄積が無い場合は空の状態から始める
		if (data.reset_history)
		{
			const FLOAT clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			context.recorder.clear_unordered_access_view_float(previous_accumlate_texture->get_unordered_access_view().Get(), clear_value);
			context.recorder.clear_unordered_access_view_float(previous_counter_texture->get_unordered_access_view().Get(), clear_value);
		}

		math::uint2 texture_size;
		texture_size.x = static_cast<UINT>(work_viewport.Width + 0.1f);
		texture_size.y = static_cast<UINT>(work_viewport.Height + 0.1f);
		{
			//	リソース類設定
			denoise_constants constant;
			constant.texture_size = texture_size;
			constant.reset_flag = ssr_data.accumalate_reset ? 1.0f : 0.0f;
			constant.denoise_velocity_max = ssr_data.velocity_reset_threshold;
			constant.denoise_color_sigma = 1.0f;
			constant.denoise_normal_sigma = 0.5f;
			constant.denoise_depth_sigma = 0.25f;
			constant.denoise_temporal_blend_factor = 0.8f;
			constant.denoise_counter_max = 10;
			constant.iteration = 0;	//	1回だけに固定しておく
			context.recorder.update_subresource(data.resources->denoise_constant_buffer.Get(), 0, 0, &constant, 0, 0);

			context.recorder.set_constant_buffers(shader_stage::compute, common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
			context.recorder.set_constant_buffers(shader_stage::compute, denoise_cbv_index, 1, data.resources->denoise_constant_buffer.GetAddressOf());
		}

		UINT thread_x = math::calc_alignment_power_of_two(texture_size.x, denoise_threads.x) / denoise_threads.x;
		UINT thread_y = math::calc_alignment_power_of_two(texture_size.y, denoise_threads.y) / denoise_threads.y;

		//	デノイズ処理
		{
			auto src_srv = work_textures[1]->get_shader_resource_view().Get();
			auto nd_srv = normal_depth_texture->get_shader_resource_view().Get();
			auto dest_uav = work_textures[0]->get_unordered_access_view().Get();
			auto accumlate_uav = accumlate_texture->get_unordered_access_view().Get();

			context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, &src_srv);
			context.recorder.set_shader_resource_views(shader_stage::compute, 1, 1, &nd_srv);

			context.recorder.set_unordered_access_views(0, 1, &dest_uav);
			context.recorder.set_unordered_access_views(1, 1, &accumlate_uav);

			context.recorder.set_compute_shader(data.resources->denoise->get_shader().Get());
			context.recorder.dispatch(thread_x, thread_y, 1);

			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
		}

		//	蓄積
		{
			auto src_srv = work_textures[0]->get_shader_resource_view().Get();
			auto accumlate_srv = previous_accumlate_texture->get_shader_resource_view().Get();
			auto vel_srv = velocity_texture->get_shader_resource_view().Get();
			auto counter_srv = previous_counter_texture->get_shader_resource_view().Get();

			auto dest_uav = work_textures[1]->get_unordered_access_view().Get();
			auto accumlate_uav = accumlate_texture->get_unordered_access_view().Get();
			auto counter_uav = counter_texture->get_unordered_access_view().Get();

			context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, &src_srv);
			context.recorder.set_shader_resource_views(shader_stage::compute, 1, 1, &accumlate_srv);
			context.recorder.set_shader_resource_views(shader_stage::compute, 2, 1, &vel_srv);
			context.recorder.set_shader_resource_views(shader_stage::compute, 3, 1, &counter_srv);

			context.recorder.set_unordered_access_views(0, 1, &dest_uav);
			context.recorder.set_unordered_access_views(1, 1, &accumlate_uav);
			context.recorder.set_unordered_access_views(2, 1, &counter_uav);

			context.recorder.set_compute_shader(data.resources->temporal_filter->get_shader().Get());
			context.recorder.dispatch(thread_x, thread_y, 1);

			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
		}

		context.recorder.clear_constant_buffers();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();
	}

	//	合成
	void render_composite(render_job_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		set_constants(context, data, resources);
		context.recorder.set_viewport(resources.get_viewport(data.work_buffers[0]));

		//	SRV設定
		bind_scene_textures(context, data, resources);
		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[0], resources.get_texture(data.work_buffers[1]));
		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[1], resources.get_texture(data.work_buffers[1]));

		context.recorder.set_pipeline(*data.resources->ssr_composite_pipeline_state);
		draw(context, data.resources->ssr_composite_pipeline_state, 4);
		context.recorder.end_pipeline();
	}

	//	合成結果をシーンに描画
	void render_write_back(render_job_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		context.recorder.set_viewport(data.scene_color_viewport);

		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.work_buffers[0]));

		context.recorder.set_pipeline(*data.resources->write_back);
		draw(context, data.resources->write_back, 4);
		context.recorder.end_pipeline();

		context.recorder.clear_constant_buffers();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_shader_slots();
	}
}

void scene_rendering::add_screen_space_reflection_pass(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const screen_space_reflection_resources>& resources)
{
	//	ポストプロセスボリュームが無ければ適応しない
	post_process_volume_render_proxy_ptr inner_volume = frame.context.render_data->post_process_volume_container.selected_volume;
	if (!inner_volume)
		return;
	auto& ssr_data = inner_volume->get_screen_space_reflection_constant();
	if (!ssr_data.is_active())
		return;

	//	@tool 蓄積バッファは視点の状態に引き継ぐ（デノイズする時だけ更新する）
	scene_view_state* view_state = frame.context.current_view_state.get();
	const bool use_denoise = ssr_data.is_flag(common_pipeline_states::screen_space_reflection_flags_step_random) && view_state;

	//	@tool 作業用テクスチャは一時テクスチャとして要求する（UV は半解像度、色・合成はシーンと同じ大きさ）
	ssr::pass_data source;
#if	defined(USE_SSR_HALF_BUFFER)
	source.uv_buffer = graph.create_texture("ssr_uv", scene_renderer_commons::make_work_texture_desc(frame.config, DXGI_FORMAT_R16G16B16A16_FLOAT, 2));
#else	//	defined(USE_SSR_HALF_BUFFER)
	source.uv_buffer = graph.create_texture("ssr_uv", scene_renderer_commons::make_work_texture_desc(frame.config, DXGI_FORMAT_R16G16B16A16_FLOAT));
#endif	//	defined(USE_SSR_HALF_BUFFER)
	const D3D11_TEXTURE2D_DESC work_desc = scene_renderer_commons::make_work_texture_desc(frame.config, DXGI_FORMAT_R16G16B16A16_FLOAT);
	source.work_buffers[0] = graph.create_texture("ssr_work[0]", work_desc);
	source.work_buffers[1] = graph.create_texture("ssr_work[1]", work_desc);
	source.resources = resources;
	source.inner_volume = inner_volume;
	source.textures = frame.textures;
	source.scene_color_viewport = frame.config.get_viewport();

	if (use_denoise)
	{
		//	@tool 蓄積バッファ（シーンと同じ大きさ）と回数（1チャンネル）
		const D3D11_TEXTURE2D_DESC accumlate_desc = work_desc;
		D3D11_TEXTURE2D_DESC counter_desc = work_desc;
		counter_desc.Format = DXGI_FORMAT_R16_FLOAT;

		//	@tool 前のフレームの物を取り込む。無い場合（最初のフレーム・大きさの変更後）は一時テクスチャをクリアして使う
		source.previous_accumlate = graph.register_external_texture("ssr_accumlate_history", view_state->get_history(ssr::accumlate_history_key), accumlate_desc);
		source.previous_counter = graph.register_external_texture("ssr_counter_history", view_state->get_history(ssr::counter_history_key), counter_desc);
		source.reset_history = !source.previous_accumlate.is_valid() || !source.previous_counter.is_valid();
		if (source.reset_history)
		{
			source.previous_accumlate = graph.create_texture("ssr_accumlate_history_cleared", accumlate_desc);
			source.previous_counter = graph.create_texture("ssr_counter_history_cleared", counter_desc);
		}
		source.accumlate = graph.create_texture("ssr_accumlate", accumlate_desc);
		source.counter = graph.create_texture("ssr_counter", counter_desc);

		//	@tool 今のフレームの蓄積を次のフレームへ引き継ぐ
		view_state->set_history(ssr::accumlate_history_key, graph.queue_extraction(source.accumlate));
		view_state->set_history(ssr::counter_history_key, graph.queue_extraction(source.counter));
	}

	const std::array<FLOAT, 4> clear_value = { 0.0f, 0.0f, 0.0f, 1.0f };

	//	SSR:UVパス
	graph.add_pass<ssr::pass_data>("screen_space_reflection_uv_pass", frame.context, [&](frame_graph::builder& builder, ssr::pass_data& data)
	{
		for (const auto& handle : frame.textures.g_buffers)
			builder.read(handle);
		builder.read(frame.textures.scene_color);
		builder.set_render_targets({ work_attachment(source.uv_buffer, clear_value) });
		builder.use_immediate_context();
		data = source;
	},
	[](const ssr::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		ssr::render_uv(context, data, resources);
	});

	//	SSR:色パス
	graph.add_pass<ssr::pass_data>("screen_space_reflection_color_pass", frame.context, [&](frame_graph::builder& builder, ssr::pass_data& data)
	{
		for (const auto& handle : frame.textures.g_buffers)
			builder.read(handle);
		builder.read(frame.textures.scene_color);
		builder.read(source.uv_buffer);
		builder.set_render_targets({ work_attachment(source.work_buffers[1], clear_value) });
		builder.use_immediate_context();
		data = source;
	},
	[](const ssr::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		ssr::render_color(context, data, resources);
	});

	//	@tool デノイズ（色→作業用[0]、蓄積へ書き、作業用[0]と前の蓄積を読んで作業用[1]・蓄積・回数へ書く）
	if (use_denoise)
	{
		graph.add_pass<ssr::pass_data>("screen_space_reflection_denoise_pass", frame.context, [&](frame_graph::builder& builder, ssr::pass_data& data)
		{
			builder.read(source.work_buffers[1]);
			builder.read(frame.textures.g_buffer(common_pipeline_states::gbuffer_id::normal_depth));
			builder.read(frame.textures.g_buffer(common_pipeline_states::gbuffer_id::velocity));
			if (source.reset_history)
			{
				builder.write(source.previous_accumlate, frame_graph::texture_usage::unordered_access);
				builder.write(source.previous_counter, frame_graph::texture_usage::unordered_access);
			}
			builder.write(source.work_buffers[0], frame_graph::texture_usage::unordered_access);
			builder.write(source.accumlate, frame_graph::texture_usage::unordered_access);
			builder.read(source.work_buffers[0]);
			builder.read(source.previous_accumlate);
			builder.read(source.previous_counter);
			builder.write(source.work_buffers[1], frame_graph::texture_usage::unordered_access);
			builder.write(source.counter, frame_graph::texture_usage::unordered_access);
			builder.use_immediate_context();
			data = source;
		},
		[](const ssr::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			ssr::render_denoise(context, data, resources);
		});
	}

	//	SSR:マージパス
	graph.add_pass<ssr::pass_data>("screen_space_reflection_composite_pass", frame.context, [&](frame_graph::builder& builder, ssr::pass_data& data)
	{
		for (const auto& handle : frame.textures.g_buffers)
			builder.read(handle);
		builder.read(frame.textures.scene_color);
		//	@tool 合成は色（またはデノイズ後の色）を読む
		builder.read(source.work_buffers[1]);
		builder.set_render_targets({ work_attachment(source.work_buffers[0], clear_value) });
		builder.use_immediate_context();
		data = source;
	},
	[](const ssr::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		ssr::render_composite(context, data, resources);
	});

	//	@tool 合成結果をシーンへ書き戻す
	graph.add_pass<ssr::pass_data>("screen_space_reflection_write_back_pass", frame.context, [&](frame_graph::builder& builder, ssr::pass_data& data)
	{
		builder.read(source.work_buffers[0]);
		builder.set_render_targets({ scene_color_attachment(frame) });
		builder.use_immediate_context();
		data = source;
	},
	[](const ssr::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		ssr::render_write_back(context, data, resources);
	});
}

#undef USE_SSR_HALF_BUFFER

#pragma endregion	//	SSR

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region DOF
namespace dof
{
	struct parameter_constants
	{
		math::float4 parameter[3];	//	x : focal_length(焦点距離)
										//	y : focus_distance(ピントの合う距離)
										//	z : circle_of_confusion(許容散乱円)
										//	w : apeture(絞り値)

										//	xy : 被写界深度の調整距離
										//	z : 空き
										//	w : デバッグフラグ

    									//	xy : 空き
    									//	z : ブラー最大半径
										//  w : スケールファクター

		math::int4 resolutions;	//  xy : シーン解像度, zw : バッファ解像度
	};
	constexpr int parameter_cbv_index = 1;
	constexpr int scene_color_srv_index = 0;
	constexpr int scene_depth_srv_index = 1;
	constexpr int sat_color_srv_index = 2;

	static constexpr math::uint3 filter_thread_count = {8, 8, 1};
	static constexpr int max_blur_radius = 64;

	static constexpr math::uint3 vertical_integrate_thread_count = {64, 1, 1};

	//	@tool 範囲総和テーブル（視点ごとに持ち、大きさが変わった時だけ作り直す。作り直した時、描画中のフレームは古い物を持ち続ける）
	struct filter_buffers
	{
		UINT width = 0;
		UINT height = 0;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> transpose_uav;
	};
	//	@tool 視点の状態に持つ名前
	constexpr view_history_key filter_buffers_key = system_utility::generate_hash("depth_of_field_filter_buffers");

	//	@tool パスのデータ
	struct pass_data
	{
		std::shared_ptr<const depth_of_field_resources> resources;
		post_process_volume_render_proxy_ptr inner_volume;
		filter_buffers buffers;
		frame_graph::texture_handle scene_color;
		frame_graph::texture_handle scene_depth;
		D3D11_VIEWPORT scene_color_viewport{};
	};

	void update_constant_buffer(render_job_context& context, const pass_data& data, const depth_of_field_data& dof_data, UINT buffer_width, UINT buffer_height)
	{
		//	リソース類設定
		parameter_constants constant;
		constant.parameter[0].x = (dof_data.focal_length);
		constant.parameter[0].y = system_units::convert_m_to_mm(dof_data.focus_distance);
		constant.parameter[0].z = (dof_data.circle_of_confusion);
		constant.parameter[0].w = (dof_data.calculate_f_number());
		constant.parameter[1].x = system_units::convert_m_to_mm(dof_data.adjust_length.x);
		constant.parameter[1].y = system_units::convert_m_to_mm(dof_data.adjust_length.y);
		constant.parameter[1].z = 0;
		constant.parameter[1].w = dof_data.display_dof_area;
		constant.parameter[2].x = 0;
		constant.parameter[2].y = 0;
		constant.parameter[2].z = static_cast<float>(std::clamp(dof_data.blur_size, 1, max_blur_radius));
		constant.parameter[2].w = float(1 << dof_data.precision_scale_factor);

		constant.resolutions.x = static_cast<int>(data.scene_color_viewport.Width);
		constant.resolutions.y = static_cast<int>(data.scene_color_viewport.Height);
		constant.resolutions.z = buffer_width;
		constant.resolutions.w = buffer_height;

		context.recorder.update_subresource(data.resources->constant_buffer.Get(), 0, 0, &constant, 0, 0);
	}

	//	@tool 範囲総和テーブルを視点の大きさに合わせて用意する（大きさが変わった時だけ作る）
	void prepare_filter_buffers(filter_buffers& buffers, const scene_renderer_commons::scene_textures_config& config)
	{
		const UINT blur_radius = max_blur_radius + 2;
		const UINT width = config.width + 2 * blur_radius;
		const UINT height = config.height + 2 * blur_radius;
		if (buffers.uav && buffers.width == width && buffers.height == height)
			return;

		HRESULT hr;
		auto device = render_data::instance()->get_device().Get();
		buffers.width = width;
		buffers.height = height;

		//	色蓄積バッファ
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_desc.StructureByteStride = sizeof(math::int4);
		buffer_desc.ByteWidth = buffer_desc.StructureByteStride * width * height;

		Microsoft::WRL::ComPtr<ID3D11Buffer> filter_buffer;
		hr = device->CreateBuffer(&buffer_desc, nullptr, filter_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateShaderResourceView(filter_buffer.Get(), nullptr, buffers.srv.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(filter_buffer.Get(), nullptr, buffers.uav.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		Microsoft::WRL::ComPtr<ID3D11Buffer> filter_buffer_transpose;
		hr = device->CreateBuffer(&buffer_desc, nullptr, filter_buffer_transpose.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(filter_buffer_transpose.Get(), nullptr, buffers.transpose_uav.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

void scene_rendering::add_depth_of_field_pass(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const depth_of_field_resources>& resources)
{
	//	ポストプロセスボリュームが無ければ適応しない
	post_process_volume_render_proxy_ptr inner_volume = frame.context.render_data->post_process_volume_container.selected_volume;
	if (!inner_volume)
		return;

	auto& depth_of_field_constant = inner_volume->get_depth_of_field_constant();
	if (!depth_of_field_constant.is_active())
		return;

	//	@tool 範囲総和テーブルは視点の状態に持つ
	scene_view_state* view_state = frame.context.current_view_state.get();
	if (!view_state)
		return;
	dof::filter_buffers& filter_buffers = view_state->get_feature_state<dof::filter_buffers>(dof::filter_buffers_key);
	dof::prepare_filter_buffers(filter_buffers, frame.config);

	graph.add_pass<dof::pass_data>("depth_of_field_pass", frame.context, [&](frame_graph::builder& builder, dof::pass_data& data)
	{
		data.scene_color = builder.read(frame.textures.scene_color);
		data.scene_depth = builder.read(frame.textures.scene_depth);
		builder.write(frame.textures.scene_color, frame_graph::texture_usage::unordered_access);
		builder.use_immediate_context();
		data.resources = resources;
		data.inner_volume = inner_volume;
		data.buffers = filter_buffers;
		data.scene_color_viewport = frame.config.get_viewport();
	},
	[](const dof::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		auto& depth_of_field_constant = data.inner_volume->get_depth_of_field_constant();
		const dof::filter_buffers& buffers = data.buffers;
		const UINT filter_buffer_width = buffers.width;
		const UINT filter_buffer_height = buffers.height;
		const texture_ptr scene_color_texture = resources.get_texture(data.scene_color);
		const texture_ptr scene_depth_texture = resources.get_texture(data.scene_depth);

		context.recorder.clear_render_target_slots();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();

		context.recorder.set_constant_buffers(shader_stage::compute, common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
		context.recorder.set_constant_buffers(shader_stage::compute, dof::parameter_cbv_index, 1, data.resources->constant_buffer.GetAddressOf());

		dof::update_constant_buffer(context, data, depth_of_field_constant, filter_buffer_width, filter_buffer_height);

		//	フィルターセットアップ
		{
			math::uint4 clear_value = {0, 0, 0, 0};
			context.recorder.clear_unordered_access_view_uint(buffers.uav.Get(), &clear_value.x);
			context.recorder.set_unordered_access_views(0, 1, buffers.uav.GetAddressOf());

			context.recorder.set_shader_resource_views(shader_stage::compute, dof::scene_color_srv_index, 1, scene_color_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, dof::scene_depth_srv_index, 1, scene_depth_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.set_compute_shader(data.resources->dof_filter_setup->get_shader().Get());
			UINT x = static_cast<UINT>( ceil(data.scene_color_viewport.Width / dof::filter_thread_count.x) + 0.1f);
			UINT y = static_cast<UINT>(ceil(data.scene_color_viewport.Height / dof::filter_thread_count.y) + 0.1f);
			context.recorder.dispatch(x, y, 1);
		}
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();

		//	範囲総和テーブル生成
		{
			context.recorder.set_compute_shader(data.resources->dof_vertical_integrate->get_shader().Get());
			//	Y
			{
				context.recorder.set_unordered_access_views(0, 1, buffers.uav.GetAddressOf());
				context.recorder.set_unordered_access_views(1, 1, buffers.transpose_uav.GetAddressOf());

				UINT x = ((filter_buffer_width + dof::vertical_integrate_thread_count.x - 1) / dof::vertical_integrate_thread_count.x);
				context.recorder.dispatch(x, 1, 1);
			}
			context.recorder.clear_unordered_access_views();

			//	X
			{
				dof::update_constant_buffer(context, data, depth_of_field_constant, filter_buffer_height, filter_buffer_width);
				context.recorder.set_unordered_access_views(0, 1, buffers.transpose_uav.GetAddressOf());
				context.recorder.set_unordered_access_views(1, 1, buffers.uav.GetAddressOf());

				UINT x = ((filter_buffer_height + dof::vertical_integrate_thread_count.x - 1) / dof::vertical_integrate_thread_count.x);
				context.recorder.dispatch(x, 1, 1);
			}
			context.recorder.clear_unordered_access_views();
		}

		//	シーンに書き戻し
		{
			dof::update_constant_buffer(context, data, depth_of_field_constant, filter_buffer_width, filter_buffer_height);
			context.recorder.set_unordered_access_views(0, 1, scene_color_texture->get_unordered_access_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, buffers.srv.GetAddressOf());

			context.recorder.set_compute_shader(data.resources->dof_apply->get_shader().Get());
			UINT x = static_cast<UINT>( ceil(data.scene_color_viewport.Width / dof::filter_thread_count.x) + 0.1f);
			UINT y = static_cast<UINT>(ceil(data.scene_color_viewport.Height / dof::filter_thread_count.y) + 0.1f);
			context.recorder.dispatch(x, y, 1);
		}

		context.recorder.clear_render_target_slots();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();
	});
}
#pragma endregion	//	DOF

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region MotionBlur
namespace motion_blur
{
	struct parameter_constants
	{
		int iteration;
		float jitter;
		float velocity_multiplyer;
		float dummy;
	};
	constexpr int cbv_index = 1;

	constexpr int tile_size = 20;

	static constexpr math::uint3 max_threads = {8, 8, 1};
	static constexpr math::uint3 blur_threads = {32, 32, 1};
}

void scene_rendering::add_motion_blur_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const motion_blur_resources>& resources)
{
	//	ポストプロセスボリュームが無ければ適応しない
	post_process_volume_render_proxy_ptr inner_volume = frame.context.render_data->post_process_volume_container.selected_volume;
	if (!inner_volume)
		return;

	auto& motion_blur_constant = inner_volume->get_motion_blur_constant();
	if (!motion_blur_constant.is_active())
		return;

	struct pass_data
	{
		std::shared_ptr<const motion_blur_resources> resources;
		post_process_volume_render_proxy_ptr inner_volume;
		frame_graph::texture_handle scene_color;
		frame_graph::texture_handle scene_depth;
		frame_graph::texture_handle velocity;
		frame_graph::texture_handle reconstract_velocity_map;
		frame_graph::texture_handle work_buffer;
		D3D11_VIEWPORT scene_color_viewport{};
	};
	graph.add_pass<pass_data>("motion_blur_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		//	タイルバッファ
		//	@tool タイルバッファとシーンの複製先は一時テクスチャとして要求する
		{
			const int width = static_cast<int>(frame.config.width);
			const int height = static_cast<int>(frame.config.height);
			D3D11_TEXTURE2D_DESC texture2d_desc{};
			texture2d_desc.Width = (width + motion_blur::tile_size - 1) / motion_blur::tile_size;
			texture2d_desc.Height = (height + motion_blur::tile_size - 1) / motion_blur::tile_size;
			texture2d_desc.MipLevels = 1;
			texture2d_desc.ArraySize = 1;
			texture2d_desc.SampleDesc.Count = 1;
			texture2d_desc.SampleDesc.Quality = 0;
			texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
			texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			texture2d_desc.CPUAccessFlags = 0;
			texture2d_desc.MiscFlags = 0;
			texture2d_desc.Format = DXGI_FORMAT_R32G32_FLOAT;
			data.reconstract_velocity_map = builder.create("reconstract_velocity", texture2d_desc);
		}
		data.work_buffer = builder.create("motion_blur_scene_copy", scene_renderer_commons::make_work_texture_desc(frame.config, DXGI_FORMAT_R16G16B16A16_FLOAT));

		data.scene_color = builder.read(frame.textures.scene_color);
		data.scene_depth = builder.read(frame.textures.scene_depth);
		data.velocity = builder.read(frame.textures.g_buffer(common_pipeline_states::gbuffer_id::velocity));
		builder.write(data.reconstract_velocity_map, frame_graph::texture_usage::unordered_access);
		builder.write(data.work_buffer, frame_graph::texture_usage::unordered_access);
		builder.read(data.reconstract_velocity_map);
		builder.read(data.work_buffer);
		builder.write(frame.textures.scene_color, frame_graph::texture_usage::unordered_access);
		builder.use_immediate_context();

		data.resources = resources;
		data.inner_volume = inner_volume;
		data.scene_color_viewport = frame.config.get_viewport();
	},
	[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		auto& motion_blur_constant = data.inner_volume->get_motion_blur_constant();
		//	@tool 一時テクスチャの実体は記録時に取得する
		const texture_ptr reconstract_velocity_texture = resources.get_texture(data.reconstract_velocity_map);
		const texture_ptr work_texture = resources.get_texture(data.work_buffer);
		const texture_ptr scene_color_texture = resources.get_texture(data.scene_color);
		const texture_ptr scene_depth_texture = resources.get_texture(data.scene_depth);
		const texture_ptr velocity_texture = resources.get_texture(data.velocity);

		context.recorder.clear_render_target_slots();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();

		//	定数バッファ更新
		{
			motion_blur::parameter_constants constant;
			constant.iteration = motion_blur_constant.iteration;
			constant.jitter = motion_blur_constant.jitter_noise_value;
			constant.velocity_multiplyer = motion_blur_constant.velocity_multiplyer;

			context.recorder.update_subresource(data.resources->constant_buffer.Get(), 0, 0, &constant, 0, 0);
		}

		//	速度バッファをタイル化して周囲の最大速度を記録
		{
			constexpr int tile_max_velocity_uav_index = 0;
			constexpr int copy_scene_uav_index = 1;

			constexpr int velocity_srv_index = 0;
			constexpr int scene_color_srv_index = 1;
			context.recorder.set_unordered_access_views(tile_max_velocity_uav_index, 1,
				reconstract_velocity_texture->get_unordered_access_view().GetAddressOf());
			context.recorder.set_unordered_access_views(copy_scene_uav_index, 1,
				work_texture->get_unordered_access_view().GetAddressOf());

			context.recorder.set_shader_resource_views(shader_stage::compute, velocity_srv_index, 1,
				velocity_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, scene_color_srv_index, 1,
				scene_color_texture->get_shader_resource_view().GetAddressOf());

			context.recorder.set_compute_shader(data.resources->reconstract_velocity->get_shader().Get());
			UINT x = (static_cast<UINT>(data.scene_color_viewport.Width + 0.1f) + motion_blur::tile_size - 1) / motion_blur::tile_size;
			UINT y = (static_cast<UINT>(data.scene_color_viewport.Height + 0.1f) + motion_blur::tile_size - 1) / motion_blur::tile_size;
			x = (x + motion_blur::max_threads.x - 1) / motion_blur::max_threads.x;
			y = (y + motion_blur::max_threads.y - 1) / motion_blur::max_threads.y;
			context.recorder.dispatch(x, y, 1);
		}
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();

		//	シーンに対してモーションブラーをかける
		{
			constexpr int scene_color_uav_index = 0;

			constexpr int scene_color_srv_index = 0;
			constexpr int scene_depth_srv_index = 1;
			constexpr int velocity_srv_index = 2;
			constexpr int neigbor_max_velocity_srv_index = 3;
			context.recorder.set_unordered_access_views(scene_color_uav_index, 1,
				scene_color_texture->get_unordered_access_view().GetAddressOf());

			context.recorder.set_shader_resource_views(shader_stage::compute, scene_color_srv_index, 1,
				work_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, scene_depth_srv_index, 1,
				scene_depth_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, velocity_srv_index, 1,
				velocity_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, neigbor_max_velocity_srv_index, 1,
				reconstract_velocity_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.set_constant_buffers(shader_stage::compute, motion_blur::cbv_index, 1, data.resources->constant_buffer.GetAddressOf());
			context.recorder.set_constant_buffers(shader_stage::compute, common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());

			context.recorder.set_compute_shader(data.resources->motion_blur->get_shader().Get());
			UINT x = ( scene_color_texture->get_width() + motion_blur::blur_threads.x - 1) / motion_blur::blur_threads.x;
			UINT y = (scene_color_texture->get_height() + motion_blur::blur_threads.y - 1) / motion_blur::blur_threads.y;
			context.recorder.dispatch(x, y, 1);
		}

		context.recorder.clear_render_target_slots();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();
	});
}
#pragma endregion	//	MotionBlur

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	@tool 共有資源の生成（ポストプロセスマテリアル・SSR・被写界深度・モーションブラー）
void scene_rendering::initialize_post_process_features(scene_render_features& features)
{
	pipeline_state_manager* instance = pipeline_state_manager::instance();

	//	ポストプロセスマテリアル
	{
		auto next = std::make_shared<post_process_resources>();
		instance->generate_pipeline_state<write_back_pipeline_state>("write_back", &next->write_back);
		features.post_process = next;
	}

	//	スクリーンスペースリフレクション
	{
		auto next = std::make_shared<screen_space_reflection_resources>();

		//	定数バッファの生成
		{
			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			buffer_desc.MiscFlags = 0;
			buffer_desc.StructureByteStride = 0;

			buffer_desc.ByteWidth = sizeof(ssr::denoise_constants);
			HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, next->denoise_constant_buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}

		//	SSR系シェーダー準備
		{
			CONST char* shader_names[] =
			{
				"screen_space_reflection_uv",
				"screen_space_reflection_color",
				"screen_space_reflection_composite",
			};
			instance->generate_pipeline_state<screen_space_reflection_uv_pipeline_state>(shader_names[0], &next->ssr_uv_pipeline_state);
			instance->generate_pipeline_state<screen_space_reflection_color_pipeline_state>(shader_names[1], &next->ssr_color_pipeline_state);
			instance->generate_pipeline_state<screen_space_reflection_composite_pipeline_state>(shader_names[2], &next->ssr_composite_pipeline_state);
		}

		//	デノイズ系シェーダー準備
		if (shader_manager* shaders = shader_manager::instance())
		{
			load_shader_cso desc;
			desc.file_path = "Assets/CSO/screen_space_reflection_denoise_cs.cso";
			shaders->load_shader<compute_shader>(next->denoise, desc);
			desc.file_path = "Assets/CSO/screen_space_reflection_temporal_filtering_cs.cso";
			shaders->load_shader<compute_shader>(next->temporal_filter, desc);
		}

		//	SSR系合成シェーダー準備
		{
			instance->generate_pipeline_state<write_back_pipeline_state>("write_back", &next->write_back);
		}
		features.screen_space_reflection = next;
	}

	//	被写界深度
	{
		auto next = std::make_shared<depth_of_field_resources>();

		//	コンピュートシェーダー生成関数
		if (shader_manager* shaders = shader_manager::instance())
		{
			load_shader_cso desc;
			desc.file_path = "Assets/CSO/dof_filter_setup_cs.cso";
			shaders->load_shader<compute_shader>(next->dof_filter_setup, desc);
			desc.file_path = "Assets/CSO/dof_vertical_integrate_cs.cso";
			shaders->load_shader<compute_shader>(next->dof_vertical_integrate, desc);
			desc.file_path = "Assets/CSO/dof_apply_cs.cso";
			shaders->load_shader<compute_shader>(next->dof_apply, desc);
		}

		//	定数バッファの生成
		{
			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			buffer_desc.MiscFlags = 0;
			buffer_desc.StructureByteStride = 0;

			buffer_desc.ByteWidth = sizeof(dof::parameter_constants);
			HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, next->constant_buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		features.depth_of_field = next;
	}

	//	モーションブラー
	{
		auto next = std::make_shared<motion_blur_resources>();

		//	コンピュートシェーダー生成関数
		if (shader_manager* shaders = shader_manager::instance())
		{
			load_shader_cso desc;
			desc.file_path = "Assets/CSO/reconstract_velocity_cs.cso";
			shaders->load_shader<compute_shader>(next->reconstract_velocity, desc);
			desc.file_path = "Assets/CSO/motion_blur_cs.cso";
			shaders->load_shader<compute_shader>(next->motion_blur, desc);
		}

		//	定数バッファの生成
		{
			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			buffer_desc.MiscFlags = 0;
			buffer_desc.StructureByteStride = 0;

			buffer_desc.ByteWidth = sizeof(motion_blur::parameter_constants);
			HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, next->constant_buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		features.motion_blur = next;
	}
}
