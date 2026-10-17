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
	void draw(const graphics_context& context,
			  graphics_pipeline_state_ptr pipeline_state,
			  UINT vertex_count,
			  D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
			  UINT vertex_offset = 0)
	{
		buffer_datas buffer_data;
		buffer_data.vertex_count = vertex_count;
		context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		context.recorder.get_device_context()->IASetPrimitiveTopology(primitive_topology);
		context.recorder.get_device_context()->Draw(vertex_count, vertex_offset);
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
	post_process_volume_snapshot_ptr inner_volume = frame.context.render_data->post_process_volume_container.selected_volume;
	if (!inner_volume)
		return;

	bool is_find = false;
	for (auto& material : inner_volume->get_materials())
	{
		if (material->get_domain() != material::domains::post_process ||
			material->get_blendable_location() != blendable_location)
			continue;
		is_find = true;
		break;
	}
	if (!is_find)
		return;

	std::string render_pass_name = std::string(magic_enum::enum_name(blendable_location));
	render_pass_name += "_post_process_material_pass";

	struct pass_data
	{
		std::shared_ptr<const post_process_resources> resources;
		std::shared_ptr<const scene_render_targets> targets;
		post_process_volume_snapshot_ptr inner_volume;
		material::blendable_locations blendable_location = material::blendable_locations::before_dof;
		frame_graph::texture_handle work_buffers[2];
		UINT work_buffer_passes[2] = {};
		UINT scene_color_pass = 0;
		D3D11_VIEWPORT viewport{};
	};
	graph.add_pass<pass_data>(render_pass_name, frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		//	@tool 作業用は一時テクスチャとして要求する
		const D3D11_TEXTURE2D_DESC work_desc = scene_renderer_commons::make_work_texture_desc(*frame.targets, DXGI_FORMAT_R16G16B16A16_FLOAT);
		data.work_buffers[0] = builder.create("post_process_work[0]", work_desc);
		data.work_buffers[1] = builder.create("post_process_work[1]", work_desc);
		builder.read(frame.textures.scene_color);
		builder.read(frame.textures.scene_depth);
		data.work_buffer_passes[0] = builder.add_render_pass({ work_attachment(data.work_buffers[0]) });
		data.work_buffer_passes[1] = builder.add_render_pass({ work_attachment(data.work_buffers[1]) });
		builder.read(data.work_buffers[0]);
		builder.read(data.work_buffers[1]);
		data.scene_color_pass = builder.add_render_pass({ scene_color_attachment(frame) });

		data.resources = resources;
		data.targets = frame.targets;
		data.inner_volume = inner_volume;
		data.blendable_location = blendable_location;
		data.viewport = frame.targets->get_scene_viewport();
	},
	[](const pass_data& data, const frame_graph::resources& resources, graphics_context& context)
	{
		//	@tool 一時テクスチャの実体は記録時に取得する
		const texture_ptr work_buffer_textures[2] = { resources.get_texture(data.work_buffers[0]), resources.get_texture(data.work_buffers[1]) };

		//	ポストプロセス
		int rendering_count = 0;
		for (auto& material : data.inner_volume->get_materials())
		{
			if (material->get_domain() != material::domains::post_process ||
				material->get_blendable_location() != data.blendable_location)
				continue;

			auto pipeline_state = material->get_pipeline_state();
			if (!pipeline_state)
				continue;

			int destination_index = (rendering_count + 1) % 2;
			int source_index = (rendering_count + 0) % 2;
			const render_pass& work_buffer_rendering_pass = resources.get_render_pass(data.work_buffer_passes[destination_index]);

			//	描画先設定
			work_buffer_rendering_pass.activate(context.recorder.get_device_context());

			//	ビューポートの設定
			context.recorder.get_device_context()->RSSetViewports(1, &data.viewport);

			//	シーン描画
			if (rendering_count == 0)
				context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, data.targets->scene_color_texture);
			else
				context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, work_buffer_textures[source_index]);
			context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_depth_map_hash_key, data.targets->scene_depth_stencil_texture);

			context.set_pipeline(*pipeline_state);
			context.bind_draw(*pipeline_state, *material, nullptr);
			draw(context, pipeline_state, 4);
			context.end_pipeline();

			work_buffer_rendering_pass.deactivate(context.recorder.get_device_context());
			++rendering_count;
		}

		//	書き戻し
		if (rendering_count > 0)
		{
			int source_index = (rendering_count + 0) % 2;
			const render_pass& scene_color_rendering_pass = resources.get_render_pass(data.scene_color_pass);
			//	描画先設定
			scene_color_rendering_pass.activate(context.recorder.get_device_context());

			//	ビューポートの設定
			context.recorder.get_device_context()->RSSetViewports(1, &data.viewport);

			//	シーン描画
			context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, work_buffer_textures[source_index]);

			context.set_pipeline(*data.resources->write_back);
			draw(context, data.resources->write_back, 4);
			context.end_pipeline();

			scene_color_rendering_pass.deactivate(context.recorder.get_device_context());
		}
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

	//	@tool パスのデータ
	struct pass_data
	{
		std::shared_ptr<const screen_space_reflection_resources> resources;
		std::shared_ptr<const scene_render_targets> targets;
		post_process_volume_snapshot_ptr inner_volume;
		bool use_denoise = false;
		bool accumlate_flip_flag = false;
		frame_graph::texture_handle uv_buffer;
		frame_graph::texture_handle work_buffers[2];
		UINT uv_pass = 0;
		UINT color_pass = 0;
		UINT composite_pass = 0;
		UINT scene_color_pass = 0;
		D3D11_VIEWPORT scene_color_viewport{};
	};

	void render_reflection(graphics_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		auto& ssr_data = data.inner_volume->get_screen_space_reflection_constant();
		const scene_render_targets& render_targets = *data.targets;
		const D3D11_VIEWPORT uv_viewport = resources.get_viewport(data.uv_buffer);
		const D3D11_VIEWPORT work_viewport = resources.get_viewport(data.work_buffers[0]);

		//	リソース類設定
		{
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
			//	@tool 固定のパイプラインステートが参照するので、パス層に登録しておく
			context.bind_table.set_constants(resource_view_bind_layer::pass, common_pipeline_states::screen_space_reflection_constant_buffer_hash_key, ssr_constant);
		}

		//	反射UV情報を算出
		{
			const render_pass& ssr_uv_pass = resources.get_render_pass(data.uv_pass);
			ssr_uv_pass.activate(context.recorder.get_device_context());
			context.recorder.get_device_context()->RSSetViewports(1, &uv_viewport);

			//	SRV設定
			for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
			{
				context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[index], render_targets.g_buffer_textures[index]);
			}
			context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[static_cast<UINT>(common_pipeline_states::gbuffer_id::base_color)], render_targets.scene_color_texture);

			context.set_pipeline(*data.resources->ssr_uv_pipeline_state);
			draw(context, data.resources->ssr_uv_pipeline_state, 4);
			context.end_pipeline();

			ssr_uv_pass.deactivate(context.recorder.get_device_context());
		}

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();

		//	反射色情報を算出
		{
			const render_pass& ssr_color_pass = resources.get_render_pass(data.color_pass);
			ssr_color_pass.activate(context.recorder.get_device_context());
			context.recorder.get_device_context()->RSSetViewports(1, &work_viewport);

			//	SRV設定
			for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
			{
				context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[index], render_targets.g_buffer_textures[index]);
			}
			context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[static_cast<UINT>(common_pipeline_states::gbuffer_id::base_color)], render_targets.scene_color_texture);
			context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[0], resources.get_texture(data.uv_buffer));

			context.set_pipeline(*data.resources->ssr_color_pipeline_state);
			draw(context, data.resources->ssr_color_pipeline_state, 4);
			context.end_pipeline();

			ssr_color_pass.deactivate(context.recorder.get_device_context());
		}

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
	}

	void render_denoise(graphics_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		auto& ssr_data = data.inner_volume->get_screen_space_reflection_constant();
		const scene_render_targets& render_targets = *data.targets;
		const bool accumlate_flip_flag = data.accumlate_flip_flag;
		const D3D11_VIEWPORT work_viewport = resources.get_viewport(data.work_buffers[0]);
		const texture_ptr work_textures[2] = { resources.get_texture(data.work_buffers[0]), resources.get_texture(data.work_buffers[1]) };

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
			context.recorder.get_device_context()->UpdateSubresource(data.resources->denoise_constant_buffer.Get(), 0, 0, &constant, 0, 0);

			context.recorder.get_device_context()->CSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
			context.recorder.get_device_context()->CSSetConstantBuffers(denoise_cbv_index, 1, data.resources->denoise_constant_buffer.GetAddressOf());
		}

		UINT thread_x = math::calc_alignment_power_of_two(texture_size.x, denoise_threads.x) / denoise_threads.x;
		UINT thread_y = math::calc_alignment_power_of_two(texture_size.y, denoise_threads.y) / denoise_threads.y;

		//	デノイズ処理
		{
			auto src_srv = work_textures[1]->get_shader_resource_view().Get();
			auto nd_srv = render_targets.g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::normal_depth)]->get_shader_resource_view().Get();
			auto dest_uav = work_textures[0]->get_unordered_access_view().Get();
			auto accumlate_uav = render_targets.ssr_accumlate_textures[accumlate_flip_flag ? 0 : 1]->get_unordered_access_view().Get();

			context.recorder.get_device_context()->CSSetShaderResources(0, 1, &src_srv);
			context.recorder.get_device_context()->CSSetShaderResources(1, 1, &nd_srv);

			context.recorder.get_device_context()->CSSetUnorderedAccessViews(0, 1, &dest_uav, nullptr);
			context.recorder.get_device_context()->CSSetUnorderedAccessViews(1, 1, &accumlate_uav, nullptr);

			context.recorder.get_device_context()->CSSetShader(data.resources->denoise->get_shader().Get(), nullptr, 0);
			context.recorder.get_device_context()->Dispatch(thread_x, thread_y, 1);

			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();
		}

		//	蓄積
		{
			auto src_srv = work_textures[0]->get_shader_resource_view().Get();
			auto accumlate_srv = render_targets.ssr_accumlate_textures[accumlate_flip_flag ? 1 : 0]->get_shader_resource_view().Get();
			auto vel_srv = render_targets.g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::velocity)]->get_shader_resource_view().Get();
			auto counter_srv = render_targets.ssr_counter_textures[accumlate_flip_flag ? 1 : 0]->get_shader_resource_view().Get();

			auto dest_uav = work_textures[1]->get_unordered_access_view().Get();
			auto accumlate_uav = render_targets.ssr_accumlate_textures[accumlate_flip_flag ? 0 : 1]->get_unordered_access_view().Get();
			auto counter_uav = render_targets.ssr_counter_textures[accumlate_flip_flag ? 0 : 1]->get_unordered_access_view().Get();

			context.recorder.get_device_context()->CSSetShaderResources(0, 1, &src_srv);
			context.recorder.get_device_context()->CSSetShaderResources(1, 1, &accumlate_srv);
			context.recorder.get_device_context()->CSSetShaderResources(2, 1, &vel_srv);
			context.recorder.get_device_context()->CSSetShaderResources(3, 1, &counter_srv);

			context.recorder.get_device_context()->CSSetUnorderedAccessViews(0, 1, &dest_uav, nullptr);
			context.recorder.get_device_context()->CSSetUnorderedAccessViews(1, 1, &accumlate_uav, nullptr);
			context.recorder.get_device_context()->CSSetUnorderedAccessViews(2, 1, &counter_uav, nullptr);

			context.recorder.get_device_context()->CSSetShader(data.resources->temporal_filter->get_shader().Get(), nullptr, 0);
			context.recorder.get_device_context()->Dispatch(thread_x, thread_y, 1);

			context.clear_shader_resorce_views();
			context.clear_unordered_access_views();
		}

		context.clear_constant_buffers();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
	}

	void render_composite(graphics_context& context, const pass_data& data, const frame_graph::resources& resources)
	{
		const scene_render_targets& render_targets = *data.targets;
		const D3D11_VIEWPORT work_viewport = resources.get_viewport(data.work_buffers[0]);

		//	合成
		{
			const render_pass& ssr_composite_pass = resources.get_render_pass(data.composite_pass);
			ssr_composite_pass.activate(context.recorder.get_device_context());
			context.recorder.get_device_context()->RSSetViewports(1, &work_viewport);

			//	SRV設定
			for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
			{
				context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[index], render_targets.g_buffer_textures[index]);
			}
			context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[static_cast<UINT>(common_pipeline_states::gbuffer_id::base_color)], render_targets.scene_color_texture);
			context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[0], resources.get_texture(data.work_buffers[1]));
			context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[1], resources.get_texture(data.work_buffers[1]));

			context.set_pipeline(*data.resources->ssr_composite_pipeline_state);
			draw(context, data.resources->ssr_composite_pipeline_state, 4);
			context.end_pipeline();

			ssr_composite_pass.deactivate(context.recorder.get_device_context());
		}

		context.clear_shader_resorce_views();

		//	合成結果をシーンに描画
		{
			const render_pass& scene_color_rendering_pass = resources.get_render_pass(data.scene_color_pass);
			scene_color_rendering_pass.activate(context.recorder.get_device_context());
			context.recorder.get_device_context()->RSSetViewports(1, &data.scene_color_viewport);

			context.bind_table.set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.work_buffers[0]));

			context.set_pipeline(*data.resources->write_back);
			draw(context, data.resources->write_back, 4);
			context.end_pipeline();

			scene_color_rendering_pass.deactivate(context.recorder.get_device_context());
		}

		context.clear_constant_buffers();
		context.clear_shader_resorce_views();
		context.clear_shader_slots();
	}
}

void scene_rendering::add_screen_space_reflection_pass(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const screen_space_reflection_resources>& resources)
{
	//	ポストプロセスボリュームが無ければ適応しない
	post_process_volume_snapshot_ptr inner_volume = frame.context.render_data->post_process_volume_container.selected_volume;
	if (!inner_volume)
		return;
	auto& ssr_data = inner_volume->get_screen_space_reflection_constant();
	if (!ssr_data.is_active())
		return;

	//	@tool 蓄積バッファの切り替えはパスを追加する時に行う（デノイズする時だけ切り替える）
	const bool use_denoise = ssr_data.is_flag(common_pipeline_states::screen_space_reflection_flags_step_random);
	bool& accumlate_flip = frame.targets->history.ssr_accumlate_flip;
	const bool flip_flag = accumlate_flip;
	if (use_denoise)
		accumlate_flip = !accumlate_flip;

	//	SSR生成
	graph.add_pass<ssr::pass_data>("screen_space_reflection_pass", frame.context, [&](frame_graph::builder& builder, ssr::pass_data& data)
	{
		//	@tool 作業用テクスチャは一時テクスチャとして要求する（UV は半解像度、色・合成はシーンと同じ大きさ）
#if	defined(USE_SSR_HALF_BUFFER)
		data.uv_buffer = builder.create("ssr_uv", scene_renderer_commons::make_work_texture_desc(*frame.targets, DXGI_FORMAT_R16G16B16A16_FLOAT, 2));
#else	//	defined(USE_SSR_HALF_BUFFER)
		data.uv_buffer = builder.create("ssr_uv", scene_renderer_commons::make_work_texture_desc(*frame.targets, DXGI_FORMAT_R16G16B16A16_FLOAT));
#endif	//	defined(USE_SSR_HALF_BUFFER)
		const D3D11_TEXTURE2D_DESC work_desc = scene_renderer_commons::make_work_texture_desc(*frame.targets, DXGI_FORMAT_R16G16B16A16_FLOAT);
		data.work_buffers[0] = builder.create("ssr_work[0]", work_desc);
		data.work_buffers[1] = builder.create("ssr_work[1]", work_desc);

		for (const auto& handle : frame.textures.g_buffers)
			builder.read(handle);
		builder.read(frame.textures.scene_color);

		const std::array<FLOAT, 4> clear_value = { 0.0f, 0.0f, 0.0f, 1.0f };
		//	SSR:UVパス
		data.uv_pass = builder.add_render_pass({ work_attachment(data.uv_buffer, clear_value) });
		builder.read(data.uv_buffer);
		//	SSR:色パス
		data.color_pass = builder.add_render_pass({ work_attachment(data.work_buffers[1], clear_value) });
		builder.read(data.work_buffers[1]);
		if (use_denoise)
		{
			//	@tool 蓄積バッファは切り替えに応じて、片方を読み、もう片方に書く
			const UINT write_index = flip_flag ? 0 : 1;
			const UINT read_index = 1 - write_index;
			//	@tool デノイズ（色→作業用[0]、蓄積へ書く）
			builder.write(data.work_buffers[0], frame_graph::texture_usage::unordered_access);
			builder.write(frame.textures.ssr_accumlate[write_index], frame_graph::texture_usage::unordered_access);
			//	@tool 蓄積（作業用[0]と前の蓄積を読み、作業用[1]・蓄積・回数へ書く）
			builder.read(data.work_buffers[0]);
			builder.read(frame.textures.ssr_accumlate[read_index]);
			builder.read(frame.textures.ssr_counter[read_index]);
			builder.write(data.work_buffers[1], frame_graph::texture_usage::unordered_access);
			builder.write(frame.textures.ssr_counter[write_index], frame_graph::texture_usage::unordered_access);
		}
		//	@tool 合成は色（またはデノイズ後の色）を読む
		builder.read(data.work_buffers[1]);
		//	SSR:マージパス
		data.composite_pass = builder.add_render_pass({ work_attachment(data.work_buffers[0], clear_value) });
		builder.read(data.work_buffers[0]);
		data.scene_color_pass = builder.add_render_pass({ scene_color_attachment(frame) });
		builder.use_immediate_context();

		data.resources = resources;
		data.targets = frame.targets;
		data.inner_volume = inner_volume;
		data.use_denoise = use_denoise;
		data.accumlate_flip_flag = flip_flag;
		data.scene_color_viewport = frame.targets->get_scene_viewport();
	},
	[](const ssr::pass_data& data, const frame_graph::resources& resources, graphics_context& context)
	{
		context.clear_constant_buffers();
		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
		context.clear_shader_slots();

		//	反射
		{
			context.debug_begin_event(L"reflection_pass");
			ssr::render_reflection(context, data, resources);
			context.debug_end_event();
		}

		//	デノイズ
		if (data.use_denoise)
		{
			context.debug_begin_event(L"denoise_pass");
			ssr::render_denoise(context, data, resources);
			context.debug_end_event();
		}

		//	合成
		{
			context.debug_begin_event(L"composite_pass");
			ssr::render_composite(context, data, resources);
			context.debug_end_event();
		}

		context.debug_end_event();
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

	//	@tool パスのデータ
	struct pass_data
	{
		std::shared_ptr<const depth_of_field_resources> resources;
		std::shared_ptr<const scene_render_targets> targets;
		post_process_volume_snapshot_ptr inner_volume;
		D3D11_VIEWPORT scene_color_viewport{};
	};

	void update_constant_buffer(graphics_context& context, const pass_data& data, const depth_of_field_data& dof_data, UINT buffer_width, UINT buffer_height)
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

		context.recorder.get_device_context()->UpdateSubresource(data.resources->constant_buffer.Get(), 0, 0, &constant, 0, 0);
	}

	//	@tool 範囲総和テーブルを視点の大きさに合わせて用意する（大きさが変わった時だけ作る）
	void prepare_filter_buffers(scene_render_targets& targets)
	{
		auto& history = targets.history;
		const UINT blur_radius = max_blur_radius + 2;
		const UINT width = targets.get_width() + 2 * blur_radius;
		const UINT height = targets.get_height() + 2 * blur_radius;
		if (history.dof_filter_buffer_uav && history.dof_filter_buffer_width == width && history.dof_filter_buffer_height == height)
			return;

		HRESULT hr;
		auto device = render_data::instance()->get_device().Get();
		history.dof_filter_buffer_width = width;
		history.dof_filter_buffer_height = height;

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
		hr = device->CreateShaderResourceView(filter_buffer.Get(), nullptr, history.dof_filter_buffer_srv.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(filter_buffer.Get(), nullptr, history.dof_filter_buffer_uav.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		Microsoft::WRL::ComPtr<ID3D11Buffer> filter_buffer_transpose;
		hr = device->CreateBuffer(&buffer_desc, nullptr, filter_buffer_transpose.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(filter_buffer_transpose.Get(), nullptr, history.dof_filter_buffer_transpose_uav.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

void scene_rendering::add_depth_of_field_pass(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const depth_of_field_resources>& resources)
{
	//	ポストプロセスボリュームが無ければ適応しない
	post_process_volume_snapshot_ptr inner_volume = frame.context.render_data->post_process_volume_container.selected_volume;
	if (!inner_volume)
		return;

	auto& depth_of_field_constant = inner_volume->get_depth_of_field_constant();
	if (!depth_of_field_constant.is_active())
		return;

	dof::prepare_filter_buffers(*frame.targets);

	graph.add_pass<dof::pass_data>("depth_of_field_pass", frame.context, [&](frame_graph::builder& builder, dof::pass_data& data)
	{
		builder.read(frame.textures.scene_color);
		builder.read(frame.textures.scene_depth);
		builder.write(frame.textures.scene_color, frame_graph::texture_usage::unordered_access);
		builder.use_immediate_context();
		data.resources = resources;
		data.targets = frame.targets;
		data.inner_volume = inner_volume;
		data.scene_color_viewport = frame.targets->get_scene_viewport();
	},
	[](const dof::pass_data& data, const frame_graph::resources&, graphics_context& context)
	{
		auto& depth_of_field_constant = data.inner_volume->get_depth_of_field_constant();
		const scene_render_targets& render_targets = *data.targets;
		const auto& history = render_targets.history;
		const UINT filter_buffer_width = history.dof_filter_buffer_width;
		const UINT filter_buffer_height = history.dof_filter_buffer_height;

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();

		context.recorder.get_device_context()->CSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
		context.recorder.get_device_context()->CSSetConstantBuffers(dof::parameter_cbv_index, 1, data.resources->constant_buffer.GetAddressOf());

		dof::update_constant_buffer(context, data, depth_of_field_constant, filter_buffer_width, filter_buffer_height);

		//	フィルターセットアップ
		{
			math::uint4 clear_value = {0, 0, 0, 0};
			context.recorder.get_device_context()->ClearUnorderedAccessViewUint(history.dof_filter_buffer_uav.Get(), &clear_value.x);
			context.recorder.get_device_context()->CSSetUnorderedAccessViews(0, 1, history.dof_filter_buffer_uav.GetAddressOf(), nullptr);

			context.recorder.get_device_context()->CSSetShaderResources(dof::scene_color_srv_index, 1, render_targets.scene_color_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.get_device_context()->CSSetShaderResources(dof::scene_depth_srv_index, 1, render_targets.scene_depth_stencil_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.get_device_context()->CSSetShader(data.resources->dof_filter_setup->get_shader().Get(), nullptr, 0);
			UINT x = static_cast<UINT>( ceil(data.scene_color_viewport.Width / dof::filter_thread_count.x) + 0.1f);
			UINT y = static_cast<UINT>(ceil(data.scene_color_viewport.Height / dof::filter_thread_count.y) + 0.1f);
			context.recorder.get_device_context()->Dispatch(x, y, 1);
		}
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();

		//	範囲総和テーブル生成
		{
			context.recorder.get_device_context()->CSSetShader(data.resources->dof_vertical_integrate->get_shader().Get(), nullptr, 0);
			//	Y
			{
				context.recorder.get_device_context()->CSSetUnorderedAccessViews(0, 1, history.dof_filter_buffer_uav.GetAddressOf(), nullptr);
				context.recorder.get_device_context()->CSSetUnorderedAccessViews(1, 1, history.dof_filter_buffer_transpose_uav.GetAddressOf(), nullptr);

				UINT x = ((filter_buffer_width + dof::vertical_integrate_thread_count.x - 1) / dof::vertical_integrate_thread_count.x);
				context.recorder.get_device_context()->Dispatch(x, 1, 1);
			}
			context.clear_unordered_access_views();

			//	X
			{
				dof::update_constant_buffer(context, data, depth_of_field_constant, filter_buffer_height, filter_buffer_width);
				context.recorder.get_device_context()->CSSetUnorderedAccessViews(0, 1, history.dof_filter_buffer_transpose_uav.GetAddressOf(), nullptr);
				context.recorder.get_device_context()->CSSetUnorderedAccessViews(1, 1, history.dof_filter_buffer_uav.GetAddressOf(), nullptr);

				UINT x = ((filter_buffer_height + dof::vertical_integrate_thread_count.x - 1) / dof::vertical_integrate_thread_count.x);
				context.recorder.get_device_context()->Dispatch(x, 1, 1);
			}
			context.clear_unordered_access_views();
		}

		//	シーンに書き戻し
		{
			dof::update_constant_buffer(context, data, depth_of_field_constant, filter_buffer_width, filter_buffer_height);
			context.recorder.get_device_context()->CSSetUnorderedAccessViews(0, 1, render_targets.scene_color_texture->get_unordered_access_view().GetAddressOf(), nullptr);
			context.recorder.get_device_context()->CSSetShaderResources(0, 1, history.dof_filter_buffer_srv.GetAddressOf());

			context.recorder.get_device_context()->CSSetShader(data.resources->dof_apply->get_shader().Get(), nullptr, 0);
			UINT x = static_cast<UINT>( ceil(data.scene_color_viewport.Width / dof::filter_thread_count.x) + 0.1f);
			UINT y = static_cast<UINT>(ceil(data.scene_color_viewport.Height / dof::filter_thread_count.y) + 0.1f);
			context.recorder.get_device_context()->Dispatch(x, y, 1);
		}

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
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
	post_process_volume_snapshot_ptr inner_volume = frame.context.render_data->post_process_volume_container.selected_volume;
	if (!inner_volume)
		return;

	auto& motion_blur_constant = inner_volume->get_motion_blur_constant();
	if (!motion_blur_constant.is_active())
		return;

	struct pass_data
	{
		std::shared_ptr<const motion_blur_resources> resources;
		std::shared_ptr<const scene_render_targets> targets;
		post_process_volume_snapshot_ptr inner_volume;
		frame_graph::texture_handle reconstract_velocity_map;
		frame_graph::texture_handle work_buffer;
		D3D11_VIEWPORT scene_color_viewport{};
	};
	graph.add_pass<pass_data>("motion_blur_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		//	タイルバッファ
		//	@tool タイルバッファとシーンの複製先は一時テクスチャとして要求する
		{
			const int width = static_cast<int>(frame.targets->get_width());
			const int height = static_cast<int>(frame.targets->get_height());
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
		data.work_buffer = builder.create("motion_blur_scene_copy", scene_renderer_commons::make_work_texture_desc(*frame.targets, DXGI_FORMAT_R16G16B16A16_FLOAT));

		builder.read(frame.textures.scene_color);
		builder.read(frame.textures.scene_depth);
		builder.read(frame.textures.g_buffer(common_pipeline_states::gbuffer_id::velocity));
		builder.write(data.reconstract_velocity_map, frame_graph::texture_usage::unordered_access);
		builder.write(data.work_buffer, frame_graph::texture_usage::unordered_access);
		builder.read(data.reconstract_velocity_map);
		builder.read(data.work_buffer);
		builder.write(frame.textures.scene_color, frame_graph::texture_usage::unordered_access);
		builder.use_immediate_context();

		data.resources = resources;
		data.targets = frame.targets;
		data.inner_volume = inner_volume;
		data.scene_color_viewport = frame.targets->get_scene_viewport();
	},
	[](const pass_data& data, const frame_graph::resources& resources, graphics_context& context)
	{
		auto& motion_blur_constant = data.inner_volume->get_motion_blur_constant();
		const scene_render_targets& render_targets = *data.targets;
		//	@tool 一時テクスチャの実体は記録時に取得する
		const texture_ptr reconstract_velocity_texture = resources.get_texture(data.reconstract_velocity_map);
		const texture_ptr work_texture = resources.get_texture(data.work_buffer);

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();

		//	定数バッファ更新
		{
			motion_blur::parameter_constants constant;
			constant.iteration = motion_blur_constant.iteration;
			constant.jitter = motion_blur_constant.jitter_noise_value;
			constant.velocity_multiplyer = motion_blur_constant.velocity_multiplyer;

			context.recorder.get_device_context()->UpdateSubresource(data.resources->constant_buffer.Get(), 0, 0, &constant, 0, 0);
		}

		//	速度バッファをタイル化して周囲の最大速度を記録
		{
			constexpr int tile_max_velocity_uav_index = 0;
			constexpr int copy_scene_uav_index = 1;

			constexpr int velocity_srv_index = 0;
			constexpr int scene_color_srv_index = 1;
			context.recorder.get_device_context()->CSSetUnorderedAccessViews(tile_max_velocity_uav_index, 1,
				reconstract_velocity_texture->get_unordered_access_view().GetAddressOf(), nullptr);
			context.recorder.get_device_context()->CSSetUnorderedAccessViews(copy_scene_uav_index, 1,
				work_texture->get_unordered_access_view().GetAddressOf(), nullptr);

			context.recorder.get_device_context()->CSSetShaderResources(velocity_srv_index, 1,
				render_targets.g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::velocity)]->get_shader_resource_view().GetAddressOf());
			context.recorder.get_device_context()->CSSetShaderResources(scene_color_srv_index, 1,
				render_targets.scene_color_texture->get_shader_resource_view().GetAddressOf());

			context.recorder.get_device_context()->CSSetShader(data.resources->reconstract_velocity->get_shader().Get(), nullptr, 0);
			UINT x = (static_cast<UINT>(data.scene_color_viewport.Width + 0.1f) + motion_blur::tile_size - 1) / motion_blur::tile_size;
			UINT y = (static_cast<UINT>(data.scene_color_viewport.Height + 0.1f) + motion_blur::tile_size - 1) / motion_blur::tile_size;
			x = (x + motion_blur::max_threads.x - 1) / motion_blur::max_threads.x;
			y = (y + motion_blur::max_threads.y - 1) / motion_blur::max_threads.y;
			context.recorder.get_device_context()->Dispatch(x, y, 1);
		}
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();

		//	シーンに対してモーションブラーをかける
		{
			constexpr int scene_color_uav_index = 0;

			constexpr int scene_color_srv_index = 0;
			constexpr int scene_depth_srv_index = 1;
			constexpr int velocity_srv_index = 2;
			constexpr int neigbor_max_velocity_srv_index = 3;
			context.recorder.get_device_context()->CSSetUnorderedAccessViews(scene_color_uav_index, 1,
				render_targets.scene_color_texture->get_unordered_access_view().GetAddressOf(), nullptr);

			context.recorder.get_device_context()->CSSetShaderResources(scene_color_srv_index, 1,
				work_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.get_device_context()->CSSetShaderResources(scene_depth_srv_index, 1,
				render_targets.scene_depth_stencil_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.get_device_context()->CSSetShaderResources(velocity_srv_index, 1,
				render_targets.g_buffer_textures[static_cast<int>(common_pipeline_states::gbuffer_id::velocity)]->get_shader_resource_view().GetAddressOf());
			context.recorder.get_device_context()->CSSetShaderResources(neigbor_max_velocity_srv_index, 1,
				reconstract_velocity_texture->get_shader_resource_view().GetAddressOf());
			context.recorder.get_device_context()->CSSetConstantBuffers(motion_blur::cbv_index, 1, data.resources->constant_buffer.GetAddressOf());
			context.recorder.get_device_context()->CSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());

			context.recorder.get_device_context()->CSSetShader(data.resources->motion_blur->get_shader().Get(), nullptr, 0);
			UINT x = ( render_targets.scene_color_texture->get_width() + motion_blur::blur_threads.x - 1) / motion_blur::blur_threads.x;
			UINT y = (render_targets.scene_color_texture->get_height() + motion_blur::blur_threads.y - 1) / motion_blur::blur_threads.y;
			context.recorder.get_device_context()->Dispatch(x, y, 1);
		}

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
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
