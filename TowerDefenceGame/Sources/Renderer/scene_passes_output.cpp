#include "Renderer/scene_render_features.h"

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

	//	@tool クリアしてから描く描画先
	frame_graph::color_attachment cleared_attachment(frame_graph::texture_handle texture)
	{
		frame_graph::color_attachment color;
		color.texture = texture;
		color.clear = true;
		return color;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region BloomAndToneMapping
namespace luminance_extract
{
	struct parameter_constants
	{
		float bloom_threshold;
		float bloom_intensity;
		float lens_ghost_threshold;
		float lens_ghost_intensity;

	    bool exposure_adjust;
		math::float3 dummy;
	};
	constexpr int cbv_index = 0;
	constexpr int bloom_uav_index = 0;
	constexpr int lens_ghost_uav_index = 1;
	constexpr int color_srv_index = 0;
	constexpr int average_luminance_srv_index = 1;
}

namespace bloom
{
	struct fft_gaussian_constants
	{
		float deviation;
		math::float3 dummy;
	};

	struct lens_ghost_constants
	{
		float dispersion_scale;
		float dispersion_offset;
		float ghost_mask_radius;
		float ghost_mask_fade;

		UINT iteration;
		UINT ghost_color_offset;
		float ghost_size;
		float dummy;
	};
	constexpr int lens_ghost_cbv_index = 0;

	constexpr int fft_gaussian_cbv_index = 0;

	static constexpr math::uint3 thread_count = {32, 32, 1};

	//	@tool パスのデータ
	struct pass_data
	{
		std::shared_ptr<const bloom_resources> resources;
		std::shared_ptr<bloom_fft_state> fft;
		post_process_volume_render_proxy_ptr inner_volume;
		frame_graph::texture_handle scene_color;
		frame_graph::texture_handle previous_luminance;
		frame_graph::texture_handle current_luminance;
		bool reset_luminance = false;	//	@tool 前のフレームの平均輝度が無い（最初のフレーム）ので、クリアしてから読む
		bool exposure_adjust = false;
	};

	void bloom_pass(render_job_context& context, const pass_data& data, const frame_graph::resources& graph_resources)
	{
		const texture_ptr scene_color_texture = graph_resources.get_texture(data.scene_color);
		const bloom_resources& resources = *data.resources;
		bloom_fft_state& fft = *data.fft;
		auto& bloom_constant = data.inner_volume->get_bloom_constant();

		//	スペクトル変換
		fft.scene_fft->solve(context.recorder, fast_fourier_transform::modes::forward);

		//	ブルームの処理モード
		bloom_data::modes mode = bloom_constant.mode;
		if (!bloom_constant.image_kernel)
		{
			mode = bloom_data::modes::standard;
		}

		switch (mode)
		{
			case bloom_data::modes::standard:
			{
				//	ガウスフィルター
				bloom::fft_gaussian_constants constant;
				constant.deviation = bloom_constant.deviation;
				context.recorder.update_subresource(resources.fft_gaussian_constant_buffer.Get(), 0, 0, &constant, 0, 0);
				context.recorder.set_constant_buffers(shader_stage::compute, bloom::fft_gaussian_cbv_index, 1, resources.fft_gaussian_constant_buffer.GetAddressOf());

				context.recorder.set_unordered_access_views(0, 1, fft.scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf());
				context.recorder.set_unordered_access_views(1, 1, fft.scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf());

				context.recorder.set_compute_shader(resources.fft_gaussian_filter->get_shader().Get());
				UINT x = (fft.scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
				UINT y = (fft.scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
				context.recorder.dispatch(x, y, 1);
				break;
			}
			case bloom_data::modes::convolution:
			{
				//	カーネルをスペクトル変換
				if (fft.bloom_image_kernel_texture != bloom_constant.image_kernel)
				{
					fft.bloom_image_kernel_texture = bloom_constant.image_kernel;
					fft.bloom_kernel_fft->clear_uavs(context.recorder);

					context.recorder.set_unordered_access_views(0, 1, fft.bloom_kernel_fft->get_complex_real()->get_unordered_access_view().GetAddressOf());
					context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, fft.bloom_image_kernel_texture->get_shader_resource_view().GetAddressOf());

					context.recorder.set_compute_shader(resources.copy_texture->get_shader().Get());
					UINT x = (fft.bloom_kernel_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
					UINT y = (fft.bloom_kernel_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
					context.recorder.dispatch(x, y, 1);

					context.recorder.clear_shader_resorce_views();
					context.recorder.clear_unordered_access_views();

					fft.bloom_kernel_fft->solve(context.recorder, fast_fourier_transform::modes::forward);
					context.recorder.clear_unordered_access_views();
				}

				context.recorder.set_unordered_access_views(0, 1, fft.scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf());
				context.recorder.set_unordered_access_views(1, 1, fft.scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf());

				context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, fft.bloom_kernel_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());
				context.recorder.set_shader_resource_views(shader_stage::compute, 1, 1, fft.bloom_kernel_fft->get_complex_imaginary()->get_shader_resource_view().GetAddressOf());

				context.recorder.set_compute_shader(resources.fft_multiply->get_shader().Get());
				UINT x = (fft.scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
				UINT y = (fft.scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
				context.recorder.dispatch(x, y, 1);
				break;
			}
		}
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();

		//	逆変換
		fft.scene_fft->solve(context.recorder, fast_fourier_transform::modes::inverse);

		//	加算合成
		{
			context.recorder.set_unordered_access_views(0, 1, scene_color_texture->get_unordered_access_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, fft.scene_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());

			context.recorder.set_compute_shader(resources.scene_additive->get_shader().Get());
			UINT x = (scene_color_texture->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (scene_color_texture->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.recorder.dispatch(x, y, 1);
			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
		}

		context.recorder.clear_render_target_slots();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();
	}

	void lens_ghost_pass(render_job_context& context, const pass_data& data, const frame_graph::resources& graph_resources)
	{
		const texture_ptr scene_color_texture = graph_resources.get_texture(data.scene_color);
		const bloom_resources& resources = *data.resources;
		bloom_fft_state& fft = *data.fft;
		auto& ghosts_constant = data.inner_volume->get_lens_ghosts_constant();

		//	UAVクリア
		fft.scene_fft->clear_uavs(context.recorder);

		//	UAVにコピー
		{
			context.recorder.set_unordered_access_views(0, 1, fft.scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, resources.lens_ghost_luminance_map->get_shader_resource_view().GetAddressOf());

			context.recorder.set_compute_shader(resources.copy_texture->get_shader().Get());
			UINT x = (fft.scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (fft.scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.recorder.dispatch(x, y, 1);

			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
		}

		//	カーネルをスペクトル変換
		if (fft.lens_ghost_image_kernel_texture != ghosts_constant.image_kernel)
		{
			fft.lens_ghost_image_kernel_texture = ghosts_constant.image_kernel;
			fft.lens_ghost_kernel_fft->clear_uavs(context.recorder);

			context.recorder.set_unordered_access_views(0, 1, fft.lens_ghost_kernel_fft->get_complex_real()->get_unordered_access_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, fft.lens_ghost_image_kernel_texture->get_shader_resource_view().GetAddressOf());

			context.recorder.set_compute_shader(resources.copy_texture->get_shader().Get());
			UINT x = (fft.lens_ghost_kernel_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (fft.lens_ghost_kernel_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.recorder.dispatch(x, y, 1);

			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
			fft.lens_ghost_kernel_fft->solve(context.recorder, fast_fourier_transform::modes::forward);
		}

		//	ゴースト生成
		{
			context.recorder.set_constant_buffers(shader_stage::compute, bloom::lens_ghost_cbv_index, 1, resources.lens_ghost_constant_buffer.GetAddressOf());
			context.recorder.set_compute_shader(resources.generate_ghost->get_shader().Get());
			context.recorder.set_shader_resource_views(shader_stage::compute, 1, 1, ghosts_constant.ghost_color_lut->get_shader_resource_view().GetAddressOf());

			UINT x = (fft.scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (fft.scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;

			bloom::lens_ghost_constants constant = {};
			constant.dispersion_scale = ghosts_constant.dispersion_scale;
			constant.dispersion_offset = ghosts_constant.dispersion_offset;
			constant.ghost_mask_radius = ghosts_constant.ghost_mask_radius;
			constant.ghost_mask_fade = ghosts_constant.ghost_mask_fade;
			constant.iteration = 4;
			constant.ghost_size = ghosts_constant.ghost_bokeh;
			constant.ghost_color_offset = 0;
			//	ピンポンブラー
			for (int index = 0; index < ghosts_constant.iteration; ++index)
			{
				constant.ghost_color_offset++;
				context.recorder.update_subresource(resources.lens_ghost_constant_buffer.Get(), 0, 0, &constant, 0, 0);

				context.recorder.set_unordered_access_views(0, 1, fft.scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf());
				context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, fft.scene_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());
				context.recorder.set_shader_resource_views(shader_stage::compute, 1, 1, ghosts_constant.ghost_color_lut->get_shader_resource_view().GetAddressOf());
				context.recorder.dispatch(x, y, 1);
				context.recorder.clear_shader_resorce_views();
				context.recorder.clear_unordered_access_views();

				context.recorder.set_unordered_access_views(0, 1, fft.scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf());
				context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, fft.scene_fft->get_complex_imaginary()->get_shader_resource_view().GetAddressOf());
				context.recorder.set_shader_resource_views(shader_stage::compute, 1, 1, ghosts_constant.ghost_color_lut->get_shader_resource_view().GetAddressOf());
				context.recorder.dispatch(x, y, 1);
				context.recorder.clear_shader_resorce_views();
				context.recorder.clear_unordered_access_views();
			}
		}

		//	スペクトル変換
		fft.scene_fft->solve(context.recorder, fast_fourier_transform::modes::forward);

		//	ゴーストサイズ調整でガウスフィルター
		if (ghosts_constant.ghost_bokeh > 1.0f)
		{
			bloom::fft_gaussian_constants constant;
			constant.deviation = std::pow(2.0f, ghosts_constant.ghost_bokeh - 1.0f);
			context.recorder.update_subresource(resources.fft_gaussian_constant_buffer.Get(), 0, 0, &constant, 0, 0);
			context.recorder.set_constant_buffers(shader_stage::compute, bloom::fft_gaussian_cbv_index, 1, resources.fft_gaussian_constant_buffer.GetAddressOf());

			context.recorder.set_unordered_access_views(0, 1, fft.scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf());
			context.recorder.set_unordered_access_views(1, 1, fft.scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf());

			context.recorder.set_compute_shader(resources.fft_gaussian_filter->get_shader().Get());
			UINT x = (fft.scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (fft.scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.recorder.dispatch(x, y, 1);

			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
		}

		//	イメージカーネル適応
		{
			context.recorder.set_unordered_access_views(0, 1, fft.scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf());
			context.recorder.set_unordered_access_views(1, 1, fft.scene_fft->get_complex_imaginary()->get_unordered_access_view().GetAddressOf());

			context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, fft.lens_ghost_kernel_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, 1, 1, fft.lens_ghost_kernel_fft->get_complex_imaginary()->get_shader_resource_view().GetAddressOf());

			context.recorder.set_compute_shader(resources.fft_multiply->get_shader().Get());
			UINT x = (fft.scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (fft.scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.recorder.dispatch(x, y, 1);
			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
		}

		//	逆変換
		fft.scene_fft->solve(context.recorder, fast_fourier_transform::modes::inverse);

		//	加算合成
		{
			context.recorder.set_unordered_access_views(0, 1, scene_color_texture->get_unordered_access_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1, fft.scene_fft->get_complex_real()->get_shader_resource_view().GetAddressOf());

			context.recorder.set_compute_shader(resources.scene_additive->get_shader().Get());
			UINT x = (scene_color_texture->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (scene_color_texture->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.recorder.dispatch(x, y, 1);
			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
		}

		context.recorder.clear_render_target_slots();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();
	}

	//	@tool 前のフレームから引き継ぐ平均輝度の名前（視点の状態に持つ）
	constexpr view_history_key average_luminance_history_key = system_utility::generate_hash("average_luminance");

	//	@tool 平均輝度バッファの形式（前のフレームの物を読み、今のフレームの物へ書いて次のフレームへ引き継ぐ）
	D3D11_TEXTURE2D_DESC make_average_luminance_desc()
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = 1;
		texture2d_desc.Height = 1;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		texture2d_desc.Format = DXGI_FORMAT_R32G32_FLOAT;
		return texture2d_desc;
	}
}

namespace tone_mapping
{
	struct parameter_constants
	{
		//  露出補正処理
		math::float4 exposure_compensation_data;  //  x : 使用フラグ, y : 適応レート, zw : 適応輝度範囲

		//  トーンマッピング
		math::float4 tone_mapping_variable[2]; //  0.xは種類として利用、以下は場合によりけり
	};
	constexpr int cbv_index = 0;

	static constexpr math::uint3 max_threads = { 32, 32, 1 };
}

void scene_rendering::add_bloom_and_tone_mapping_passes(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const bloom_resources>& resources, const std::shared_ptr<bloom_fft_state>& fft)
{
	//	ポストプロセスボリュームが無ければ適応しない
	post_process_volume_render_proxy_ptr inner_volume = frame.context.render_data->post_process_volume_container.selected_volume;
	if (!inner_volume)
		return;

	auto& bloom_constant = inner_volume->get_bloom_constant();
	auto& lens_ghosts_data = inner_volume->get_lens_ghosts_constant();

	//	@tool 平均輝度は視点ごとに前のフレームの値から順応させる（前のフレームの物を視点の状態から取り込み、今のフレームの物を引き継ぐ）
	scene_view_state* view_state = frame.context.current_view_state.get();
	const D3D11_TEXTURE2D_DESC average_luminance_desc = bloom::make_average_luminance_desc();
	frame_graph::texture_handle previous_luminance;
	if (view_state)
		previous_luminance = graph.register_external_texture("average_luminance_history", view_state->get_history(bloom::average_luminance_history_key), average_luminance_desc);
	const bool reset_luminance = !previous_luminance.is_valid();
	if (reset_luminance)
		previous_luminance = graph.create_texture("average_luminance_history_cleared", average_luminance_desc);
	const frame_graph::texture_handle current_luminance = graph.create_texture("average_luminance", average_luminance_desc);
	if (view_state)
		view_state->set_history(bloom::average_luminance_history_key, graph.queue_extraction(current_luminance));

	//	平均輝度
	bool exposure_adjust = inner_volume->get_tone_mapping_constant().get_tone_map_type() != tone_mapping_data::tone_mapping_type::none;

	const frame_graph::texture_handle lens_ghost_luminance = graph.import_texture("lens_ghost_luminance_map", resources->lens_ghost_luminance_map);

	bloom::pass_data source;
	source.resources = resources;
	source.fft = fft;
	source.inner_volume = inner_volume;
	source.scene_color = frame.textures.scene_color;
	source.previous_luminance = previous_luminance;
	source.current_luminance = current_luminance;
	source.reset_luminance = reset_luminance;
	source.exposure_adjust = exposure_adjust;

	//	平均輝度算出
	graph.add_pass<bloom::pass_data>("average_exposure_pass", frame.context, [&](frame_graph::builder& builder, bloom::pass_data& data)
	{
		builder.read(frame.textures.scene_color);
		//	@tool 前のフレームの平均輝度が無い場合は、クリアしてから読む
		if (reset_luminance)
			builder.write(previous_luminance, frame_graph::texture_usage::unordered_access);
		builder.read(previous_luminance);
		builder.write(current_luminance, frame_graph::texture_usage::unordered_access);
		builder.use_immediate_context();
		data = source;
	},
	[](const bloom::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		auto& tone_mapping_constant = data.inner_volume->get_tone_mapping_constant();
		auto& exposure_compensation_constant = data.inner_volume->get_exposure_compensation_constant();
		const texture_ptr previous_luminance_texture = resources.get_texture(data.previous_luminance);
		const texture_ptr current_luminance_texture = resources.get_texture(data.current_luminance);

		context.recorder.clear_render_target_slots();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();

		if (data.reset_luminance)
		{
			const FLOAT clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			context.recorder.clear_unordered_access_view_float(previous_luminance_texture->get_unordered_access_view().Get(), clear_value);
		}

		//	定数バッファ更新
		{
			tone_mapping::parameter_constants constant;
			constant.exposure_compensation_data.x = exposure_compensation_constant.is_active() ? 1.0f : 0.0f;
			float elapsed_time = context.scene_constant.time_data.x - context.scene_constant.time_data.y;
			float elapsed_frame = system_units::convert_sec_to_fps(elapsed_time);
			constant.exposure_compensation_data.y = exposure_compensation_constant.adapt_rate * elapsed_frame;
			constant.exposure_compensation_data.z = exposure_compensation_constant.luminance_range.x;
			constant.exposure_compensation_data.w = exposure_compensation_constant.luminance_range.y;

			constant.tone_mapping_variable[0].x = static_cast<float>(tone_mapping_constant.get_tone_map_type());
			constant.tone_mapping_variable[0].y = tone_mapping_constant.tone_mapping_variables[0];
			constant.tone_mapping_variable[0].z = tone_mapping_constant.tone_mapping_variables[1];
			constant.tone_mapping_variable[0].w = tone_mapping_constant.tone_mapping_variables[2];
			constant.tone_mapping_variable[1].x = tone_mapping_constant.tone_mapping_variables[3];
			constant.tone_mapping_variable[1].y = tone_mapping_constant.tone_mapping_variables[4];
			constant.tone_mapping_variable[1].z = tone_mapping_constant.tone_mapping_variables[5];
			constant.tone_mapping_variable[1].w = tone_mapping_constant.tone_mapping_variables[6];

			context.recorder.update_subresource(data.resources->tone_mapping_constant_buffer.Get(), 0, 0, &constant, 0, 0);
		}

		//	平均輝度算出
		context.recorder.set_constant_buffers(shader_stage::compute, tone_mapping::cbv_index, 1,
			data.resources->tone_mapping_constant_buffer.GetAddressOf());
		context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1,
			resources.get_texture(data.scene_color)->get_shader_resource_view().GetAddressOf());
		context.recorder.set_shader_resource_views(shader_stage::compute, 1, 1,
			previous_luminance_texture->get_shader_resource_view().GetAddressOf());

		context.recorder.set_unordered_access_views(0, 1,
			current_luminance_texture->get_unordered_access_view().GetAddressOf());

		context.recorder.set_compute_shader(data.resources->correct_luminance->get_shader().Get());
		context.recorder.dispatch(1, 1, 1);

		context.recorder.clear_render_target_slots();
		context.recorder.clear_shader_resorce_views();
		context.recorder.clear_unordered_access_views();
	});

	//	輝度値算出
	if (bloom_constant.is_active() || lens_ghosts_data.is_active())
	{
		graph.add_pass<bloom::pass_data>("luminance_extract_pass", frame.context, [&](frame_graph::builder& builder, bloom::pass_data& data)
		{
			builder.read(frame.textures.scene_color);
			builder.read(current_luminance);
			builder.write(lens_ghost_luminance, frame_graph::texture_usage::unordered_access);
			//	@tool FFT の作業領域（グラフの外）にも書くので除かない
			builder.side_effect();
			builder.use_immediate_context();
			data = source;
		},
		[](const bloom::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			bloom_fft_state& fft = *data.fft;
			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();

			fft.scene_fft->clear_uavs(context.recorder);

			auto& bloom_constant = data.inner_volume->get_bloom_constant();
			auto& lens_ghosts_data = data.inner_volume->get_lens_ghosts_constant();
			luminance_extract::parameter_constants constant;
			constant.bloom_threshold = bloom_constant.threshold;
			constant.bloom_intensity = bloom_constant.is_active() ? bloom_constant.intensity : -1.0f;
			constant.lens_ghost_threshold = lens_ghosts_data.threshold;
			constant.lens_ghost_intensity = lens_ghosts_data.is_active() ? lens_ghosts_data.intensity : -1.0f;
			constant.exposure_adjust = data.exposure_adjust;
			context.recorder.update_subresource(data.resources->luminance_extract_constant_buffer.Get(), 0, 0, &constant, 0, 0);
			context.recorder.set_constant_buffers(shader_stage::compute, luminance_extract::cbv_index, 1, data.resources->luminance_extract_constant_buffer.GetAddressOf());

			context.recorder.set_unordered_access_views(luminance_extract::bloom_uav_index, 1,
						fft.scene_fft->get_complex_real()->get_unordered_access_view().GetAddressOf());
			context.recorder.set_unordered_access_views(luminance_extract::lens_ghost_uav_index, 1,
						data.resources->lens_ghost_luminance_map->get_unordered_access_view().GetAddressOf());

			context.recorder.set_shader_resource_views(shader_stage::compute, luminance_extract::color_srv_index, 1,
						resources.get_texture(data.scene_color)->get_shader_resource_view().GetAddressOf());
			context.recorder.set_shader_resource_views(shader_stage::compute, luminance_extract::average_luminance_srv_index, 1,
						resources.get_texture(data.current_luminance)->get_shader_resource_view().GetAddressOf());

			context.recorder.set_compute_shader(data.resources->luminance_extract->get_shader().Get());
			UINT x = (fft.scene_fft->get_complex_real()->get_width() + bloom::thread_count.x - 1) / bloom::thread_count.x;
			UINT y = (fft.scene_fft->get_complex_real()->get_height() + bloom::thread_count.y - 1) / bloom::thread_count.y;
			context.recorder.dispatch(x, y, 1);
			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
		});
	}

	//	ブルーム処理
	if (bloom_constant.is_active())
	{
		graph.add_pass<bloom::pass_data>("bloom_pass", frame.context, [&](frame_graph::builder& builder, bloom::pass_data& data)
		{
			builder.read(frame.textures.scene_color);
			builder.write(frame.textures.scene_color, frame_graph::texture_usage::unordered_access);
			builder.use_immediate_context();
			data = source;
		},
		[](const bloom::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			bloom::bloom_pass(context, data, resources);
		});
	}

	//	レンズゴースト
	if (lens_ghosts_data.is_active())
	{
		graph.add_pass<bloom::pass_data>("lens_ghost_pass", frame.context, [&](frame_graph::builder& builder, bloom::pass_data& data)
		{
			builder.read(frame.textures.scene_color);
			builder.read(lens_ghost_luminance);
			builder.write(frame.textures.scene_color, frame_graph::texture_usage::unordered_access);
			builder.use_immediate_context();
			data = source;
		},
		[](const bloom::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			bloom::lens_ghost_pass(context, data, resources);
		});
	}

	//	トーンマッピング
	if (exposure_adjust)
	{
		graph.add_pass<bloom::pass_data>("tone_mapping_pass", frame.context, [&](frame_graph::builder& builder, bloom::pass_data& data)
		{
			builder.read(frame.textures.scene_color);
			builder.read(current_luminance);
			builder.write(frame.textures.scene_color, frame_graph::texture_usage::unordered_access);
			builder.use_immediate_context();
			data = source;
		},
		[](const bloom::pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			const texture_ptr scene_color_texture = resources.get_texture(data.scene_color);

			context.recorder.clear_render_target_slots();
			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();

			context.recorder.set_constant_buffers(shader_stage::compute, tone_mapping::cbv_index, 1,
				data.resources->tone_mapping_constant_buffer.GetAddressOf());

			context.recorder.set_shader_resource_views(shader_stage::compute, 0, 1,
				resources.get_texture(data.current_luminance)->get_shader_resource_view().GetAddressOf());

			context.recorder.set_unordered_access_views(0, 1,
				scene_color_texture->get_unordered_access_view().GetAddressOf());

			context.recorder.set_compute_shader(data.resources->tone_mapping->get_shader().Get());
			UINT x = scene_color_texture->get_width();
			UINT y = scene_color_texture->get_height();
			x = (x + tone_mapping::max_threads.x - 1) / tone_mapping::max_threads.x;
			y = (y + tone_mapping::max_threads.y - 1) / tone_mapping::max_threads.y;
			context.recorder.dispatch(x, y, 1);

			context.recorder.clear_render_target_slots();
			context.recorder.clear_shader_resorce_views();
			context.recorder.clear_unordered_access_views();
		});
	}
}
#pragma endregion	//	BloomAndToneMapping

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region Antialias
void scene_rendering::add_anti_alias_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const anti_alias_resources>& resources)
{
	//	@tool 設定は抽出時点の値を使う
	const auto mode = frame.context.render_data->settings.antialias_mode;
	if (mode != render_settings::antialias_modes::fxaa && mode != render_settings::antialias_modes::smaa)
		return;

	//	@tool パスのデータ（FXAA は2パス、SMAA は4パスに分けて、各パスが同じ物を持つ）
	struct pass_data
	{
		std::shared_ptr<const anti_alias_resources> resources;
		frame_graph::texture_handle scene_color;
		frame_graph::texture_handle work_color;
		frame_graph::texture_handle edge_buffer;
		frame_graph::texture_handle blend_buffer;
		D3D11_VIEWPORT scene_color_viewport{};
	};

	//	@tool 作業用テクスチャは一時テクスチャとして要求する
	pass_data source;
	source.resources = resources;
	source.scene_color = frame.textures.scene_color;
	source.work_color = graph.create_texture("anti_alias_work", scene_renderer_commons::make_work_texture_desc(frame.config, DXGI_FORMAT_R16G16B16A16_FLOAT));
	source.scene_color_viewport = frame.config.get_viewport();

	if (mode == render_settings::antialias_modes::fxaa)
	{
		//	@tool FXAA（シーン→作業用）
		graph.add_pass<pass_data>("fxaa_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
		{
			builder.read(source.scene_color);
			builder.set_render_targets({ cleared_attachment(source.work_color) });
			data = source;
		},
		[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			const anti_alias_resources& aa = *data.resources;
			context.recorder.set_viewport(resources.get_viewport(data.work_color));

			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.scene_color));

			context.recorder.set_pipeline(*aa.fxaa);
			draw(context, aa.fxaa, 4);
			context.recorder.end_pipeline();
		});
	}
	else
	{
		//	SMAA用バッファ
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = frame.config.width;
		texture2d_desc.Height = frame.config.height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		//	Edge
		texture2d_desc.Format = DXGI_FORMAT_R16G16_FLOAT;
		source.edge_buffer = graph.create_texture("smaa_edge_buffer", texture2d_desc);
		//	BlendWeight
		texture2d_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		source.blend_buffer = graph.create_texture("smaa_blend_buffer", texture2d_desc);

		//	エッジ検出
		graph.add_pass<pass_data>("smaa_edge_detection_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
		{
			builder.read(source.scene_color);
			builder.set_render_targets({ cleared_attachment(source.edge_buffer) });
			data = source;
		},
		[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			const anti_alias_resources& aa = *data.resources;
			context.recorder.set_viewport(resources.get_viewport(data.edge_buffer));

			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.scene_color));

			context.recorder.set_pipeline(*aa.smaa_edge_detection);
			draw(context, aa.smaa_edge_detection, 4);
			context.recorder.end_pipeline();
		});

		//	ブレンドウェイト算出
		graph.add_pass<pass_data>("smaa_blending_weight_calculation_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
		{
			builder.read(source.edge_buffer);
			builder.set_render_targets({ cleared_attachment(source.blend_buffer) });
			data = source;
		},
		[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			const anti_alias_resources& aa = *data.resources;
			context.recorder.set_viewport(resources.get_viewport(data.blend_buffer));

			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.edge_buffer));

			context.recorder.set_pipeline(*aa.smaa_blending_weight_calculation);
			draw(context, aa.smaa_blending_weight_calculation, 4);
			context.recorder.end_pipeline();
		});

		//	合成
		graph.add_pass<pass_data>("smaa_neighborhood_blending_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
		{
			builder.read(source.scene_color);
			builder.read(source.blend_buffer);
			builder.read(source.edge_buffer);
			builder.set_render_targets({ cleared_attachment(source.work_color) });
			data = source;
		},
		[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			const anti_alias_resources& aa = *data.resources;
			context.recorder.set_viewport(resources.get_viewport(data.work_color));

			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.scene_color));
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[0], resources.get_texture(data.blend_buffer));
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[1], resources.get_texture(data.edge_buffer));

			context.recorder.set_pipeline(*aa.smaa_neighborhood_blending);
			draw(context, aa.smaa_neighborhood_blending, 4);
			context.recorder.end_pipeline();
		});
	}

	//	シーンへ合成
	graph.add_pass<pass_data>("anti_alias_write_back_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		builder.read(source.work_color);
		if (source.blend_buffer.is_valid())
			builder.read(source.blend_buffer);
		if (source.edge_buffer.is_valid())
			builder.read(source.edge_buffer);
		builder.set_render_targets({ cleared_attachment(source.scene_color) });
		data = source;
	},
	[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		const anti_alias_resources& aa = *data.resources;
		context.recorder.set_viewport(data.scene_color_viewport);

		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.work_color));
		if (data.blend_buffer.is_valid())
		{
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[0], resources.get_texture(data.blend_buffer));
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::work_texture_hash_keys[1], resources.get_texture(data.edge_buffer));
		}

		context.recorder.set_pipeline(*aa.write_back);
		draw(context, aa.write_back, 4);
		context.recorder.end_pipeline();
	});
}
#pragma endregion	//	AntiAlias

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region FinalPass
void scene_rendering::add_final_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const final_pass_resources>& resources)
{
	struct pass_data
	{
		std::shared_ptr<const final_pass_resources> resources;
		frame_graph::texture_handle scene_color;
		D3D11_VIEWPORT viewport{};
	};
	graph.add_pass<pass_data>("final_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		//	@tool 出力先（バックバッファ、またはビューの出力テクスチャ）へ書く
		data.scene_color = builder.read(frame.textures.scene_color);
		frame_graph::color_attachment output;
		output.texture = frame.textures.output;
		output.external_view = frame.output.view;
		output.clear = true;
		builder.set_render_targets({ output });
		data.viewport = frame.output.viewport;
		data.resources = resources;
	},
	[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		context.recorder.set_viewport(data.viewport);
		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_color_map_hash_key, resources.get_texture(data.scene_color));

		context.recorder.set_pipeline(*data.resources->convert_linear_to_srgb_pipeline_states);
		draw(context, data.resources->convert_linear_to_srgb_pipeline_states, 4);
		context.recorder.end_pipeline();
	});
}
#pragma endregion	//	FinalPass

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	処理負荷軽減のために低解像度版でやる
#define USE_HALF_FFT
#if defined(USE_HALF_FFT)
#define FFT_TEXTURE_SIZE (fast_fourier_transform::sizes::_512)
#define FFT_TEXTURE_WIDTH (512)
#define FFT_TEXTURE_HEIGHT (512)
#else	//	defined(USE_HALF_FFT)
#define FFT_TEXTURE_SIZE (fast_fourier_transform::sizes::_1024)
#define FFT_TEXTURE_WIDTH (1024)
#define FFT_TEXTURE_HEIGHT (1024)
#endif	//	defined(USE_HALF_FFT)

//	@tool 共有資源の生成（ブルームとトーンマッピング・アンチエイリアス・ファイナルパス）
void scene_rendering::initialize_output_features(scene_render_features& features)
{
	pipeline_state_manager* instance = pipeline_state_manager::instance();

	//	ブルームとトーンマッピング
	{
		auto next = std::make_shared<bloom_resources>();

		//	高速フーリエ変換クラス
		auto fft = std::make_shared<bloom_fft_state>();
		fft->scene_fft = std::make_unique<fast_fourier_transform>(FFT_TEXTURE_SIZE);
		fft->bloom_kernel_fft = std::make_unique<fast_fourier_transform>(FFT_TEXTURE_SIZE);
		fft->lens_ghost_kernel_fft = std::make_unique<fast_fourier_transform>(FFT_TEXTURE_SIZE);
		features.bloom_fft = fft;

		//	コンピュートシェーダー生成関数
		if (shader_manager* shaders = shader_manager::instance())
		{
			load_shader_cso desc;
			desc.file_path = "Assets/CSO/correct_luminance_cs.cso";
			shaders->load_shader<compute_shader>(next->correct_luminance, desc);
			desc.file_path = "Assets/CSO/tone_mapping_cs.cso";
			shaders->load_shader<compute_shader>(next->tone_mapping, desc);

			desc.file_path = "Assets/CSO/fast_fourier_transform_gaussian_filter_cs.cso";
			shaders->load_shader<compute_shader>(next->fft_gaussian_filter, desc);
			desc.file_path = "Assets/CSO/fast_fourier_transform_complex_multiply_cs.cso";
			shaders->load_shader<compute_shader>(next->fft_multiply, desc);

			desc.file_path = "Assets/CSO/resize_copy_cs.cso";
			shaders->load_shader<compute_shader>(next->copy_texture, desc);
			desc.file_path = "Assets/CSO/bloom_additional_half_cs.cso";
			shaders->load_shader<compute_shader>(next->scene_additive, desc);

			desc.file_path = "Assets/CSO/luminance_extract_cs.cso";
			shaders->load_shader<compute_shader>(next->luminance_extract, desc);

			desc.file_path = "Assets/CSO/generate_ghost_cs.cso";
			shaders->load_shader<compute_shader>(next->generate_ghost, desc);
		}

		//	定数バッファの生成
		if (auto device = render_data::instance()->get_device())
		{
			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			buffer_desc.MiscFlags = 0;
			buffer_desc.StructureByteStride = 0;

			buffer_desc.ByteWidth = sizeof(tone_mapping::parameter_constants);
			HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, next->tone_mapping_constant_buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			buffer_desc.ByteWidth = sizeof(luminance_extract::parameter_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, next->luminance_extract_constant_buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			buffer_desc.ByteWidth = sizeof(bloom::fft_gaussian_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, next->fft_gaussian_constant_buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			buffer_desc.ByteWidth = sizeof(bloom::lens_ghost_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, next->lens_ghost_constant_buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}

		//	レンズゴースト用の輝度
		if (auto tex_manager = texture_manager::instance())
		{
			D3D11_TEXTURE2D_DESC texture2d_desc{};
			texture2d_desc.Width = FFT_TEXTURE_WIDTH;
			texture2d_desc.Height = FFT_TEXTURE_HEIGHT;
			texture2d_desc.MipLevels = 1;
			texture2d_desc.ArraySize = 1;
			texture2d_desc.SampleDesc.Count = 1;
			texture2d_desc.SampleDesc.Quality = 0;
			texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
			texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			texture2d_desc.CPUAccessFlags = 0;
			texture2d_desc.MiscFlags = 0;
			texture2d_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			tex_manager->generate_unregistered_texture2d(next->lens_ghost_luminance_map, texture2d_desc);
		}
		features.bloom = next;
	}

	//	アンチエイリアス
	{
		auto next = std::make_shared<anti_alias_resources>();
		//	FXAA
		instance->generate_pipeline_state<fxaa_pipeline_state>("fxaa", &next->fxaa);
		//	SMAA
		instance->generate_pipeline_state<smaa_edge_detection_pipeline_state>("smaa_edge_detection", &next->smaa_edge_detection);
		instance->generate_pipeline_state<smaa_blending_weight_calculation_pipeline_state>("smaa_blending_weight_calculation", &next->smaa_blending_weight_calculation);
		instance->generate_pipeline_state<smaa_neighborhood_blending_pipeline_state>("smaa_neighborhood_blending", &next->smaa_neighborhood_blending);
		instance->generate_pipeline_state<write_back_pipeline_state>("write_back", &next->write_back);
		features.anti_alias = next;
	}

	//	ファイナルパス
	{
		auto next = std::make_shared<final_pass_resources>();
		instance->generate_pipeline_state<convert_linear_to_srgb_pipeline_state>("convert_linear_to_srgb", &next->convert_linear_to_srgb_pipeline_states);
		next->convert_linear_to_srgb_pipeline_states->set_domain(pipeline_state_base::domains::system);
		features.final_pass = next;
	}
}
