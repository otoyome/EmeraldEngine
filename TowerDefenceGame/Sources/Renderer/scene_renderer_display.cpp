#include "Renderer/scene_renderer_display.h"
#include "Graphics/global_shader.h"

#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "Graphics/color_space.h"
#include "Graphics/light_utility.h"
#include "System/engine_config_data.h"
#include "Graphics/post_process_volume.h"

namespace
{
	//	グローバルシェーダー
	global_shader<compute_shader> global_average_luminance_cs("Engine/EngineShader/Postprocess/Exposure/average_luminance_cs.hlsl");
	global_shader<compute_shader> global_exposure_adjust_cs("Engine/EngineShader/Postprocess/Exposure/exposure_adjust_cs.hlsl");
	global_shader<compute_shader> global_tone_mapping_cs("Engine/EngineShader/Postprocess/ToneMapping/tone_mapping_cs.hlsl");

	//	露出用の定数
	struct exposure_constants
	{
		math::uint2 buffer_size;
		float exposure_scale;
		UINT metering_mode;
		math::float4 luminance_coefficients;
		float luminous_efficacy;
		math::float3 dummy;
	};

	//	トーンマッピング用の定数
	struct tone_mapping_constants
	{
		math::uint2 buffer_size;
		float peak_nits;
		float luminous_efficacy;
		math::float4 luminance_coefficients;
	};

	//	ディスプレイマッピング用の定数
	struct display_mapping_constants
	{
		math::float4 rendering_to_output[3];
		float peak_nits;
		float luminous_efficacy;
		UINT output_curve;
		float dummy;
	};

	constexpr math::uint2 threads = { 16, 16 };

	void draw(const graphics_context& context, UINT vertex_count)
	{
		context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		context.device_context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		context.device_context->Draw(vertex_count, 0);
	}

	Microsoft::WRL::ComPtr<ID3D11Buffer> create_constant_buffer(UINT size)
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.ByteWidth = size;
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		return buffer;
	}

	//	描画色域の輝度係数
	math::float4 get_rendering_luminance_coefficients()
	{
		const math::vector3f coefficients = color_space::get_luminance_coefficients(engine_config_data::instance()->get_active_rendering_gamut());
		return { coefficients[0], coefficients[1], coefficients[2], 0.0f };
	}

	//	露出の設定(ポストプロセスボリュームが無ければ既定値)
	exposure_settings get_exposure_settings(const renderer_context& context)
	{
		if (post_process_volume_ptr volume = context.render_data->post_process_volume_container.selected_volume)
			return volume->get_exposure_settings();
		return exposure_settings();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
exposure_renderer::exposure_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
	average_luminance = global_average_luminance_cs.get();
	exposure_adjust = global_exposure_adjust_cs.get();

	constant_buffer = create_constant_buffer(sizeof(exposure_constants));

	//	平均輝度の結果
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_desc.ByteWidth = sizeof(float);
		buffer_desc.StructureByteStride = sizeof(float);
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, average_luminance_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
		uav_desc.Format = DXGI_FORMAT_UNKNOWN;
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.NumElements = 1;
		hr = render_data::instance()->get_device()->CreateUnorderedAccessView(average_luminance_buffer.Get(), &uav_desc, average_luminance_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

exposure_renderer::~exposure_renderer()
{
}

void exposure_renderer::render(renderer_context& renderer_context)
{
	const exposure_settings settings = get_exposure_settings(renderer_context);
	job_executor.execute([this, settings](graphics_context& context)
	{
		//	前のフレームの測光結果で自動露出を進める
		if (!initialized)
		{
			controller.reset(settings);
			initialized = true;
		}
		const float elapsed_time = context.scene_constant.time_data.x - context.scene_constant.time_data.y;
		controller.update(settings, measured_luminance.load(), elapsed_time);

		const UINT width = static_cast<UINT>(render_targets->scene_color_texture->get_width());
		const UINT height = static_cast<UINT>(render_targets->scene_color_texture->get_height());

		exposure_constants constants{};
		constants.buffer_size = { width, height };
		constants.exposure_scale = controller.calculate_exposure_scale() * sdr_reference_white_nits;
		constants.metering_mode = static_cast<UINT>(settings.metering_mode);
		constants.luminance_coefficients = get_rendering_luminance_coefficients();
		constants.luminous_efficacy = static_cast<float>(light_utility::luminous_efficacy);
		context.device_context->UpdateSubresource(constant_buffer.Get(), 0, nullptr, &constants, 0, 0);

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
		context.device_context->CSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());

		//	測光
		context.device_context->CSSetShaderResources(0, 1, render_targets->scene_color_texture->get_shader_resource_view().GetAddressOf());
		context.device_context->CSSetUnorderedAccessViews(0, 1, average_luminance_view.GetAddressOf(), nullptr);
		context.device_context->CSSetShader(average_luminance->get_shader().Get(), nullptr, 0);
		context.device_context->Dispatch(1, 1, 1);
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();

		render_data::instance()->get_readback_queue().read_back(context.device_context, average_luminance_buffer.Get(), sizeof(float),
			[this](const void* data, UINT size_in_bytes)
			{
				if (size_in_bytes >= sizeof(float))
					measured_luminance.store(*static_cast<const float*>(data));
			});

		//	露光補正
		context.device_context->CSSetUnorderedAccessViews(0, 1, render_targets->scene_color_texture->get_unordered_access_view().GetAddressOf(), nullptr);
		context.device_context->CSSetShader(exposure_adjust->get_shader().Get(), nullptr, 0);
		context.device_context->Dispatch((width + threads.x - 1) / threads.x, (height + threads.y - 1) / threads.y, 1);

		context.clear_unordered_access_views();
		context.clear_constant_buffers();
	}, "exposure_pass", renderer_context, get_render_basis_sort_order(), true);
}

void exposure_renderer::resize_buffer(int width, int height)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
tone_mapping_renderer::tone_mapping_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
	tone_mapping = global_tone_mapping_cs.get();
	constant_buffer = create_constant_buffer(sizeof(tone_mapping_constants));
}

tone_mapping_renderer::~tone_mapping_renderer()
{
}

void tone_mapping_renderer::render(renderer_context& renderer_context)
{
	job_executor.execute([this](graphics_context& context)
	{
		const UINT width = static_cast<UINT>(render_targets->scene_color_texture->get_width());
		const UINT height = static_cast<UINT>(render_targets->scene_color_texture->get_height());

		tone_mapping_constants constants{};
		constants.buffer_size = { width, height };
		constants.peak_nits = sdr_reference_white_nits;
		constants.luminous_efficacy = static_cast<float>(light_utility::luminous_efficacy);
		constants.luminance_coefficients = get_rendering_luminance_coefficients();
		context.device_context->UpdateSubresource(constant_buffer.Get(), 0, nullptr, &constants, 0, 0);

		context.clear_render_target_slots();
		context.clear_shader_resorce_views();
		context.clear_unordered_access_views();
		context.device_context->CSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
		context.device_context->CSSetUnorderedAccessViews(0, 1, render_targets->scene_color_texture->get_unordered_access_view().GetAddressOf(), nullptr);
		context.device_context->CSSetShader(tone_mapping->get_shader().Get(), nullptr, 0);
		context.device_context->Dispatch((width + threads.x - 1) / threads.x, (height + threads.y - 1) / threads.y, 1);
		context.clear_unordered_access_views();
		context.clear_constant_buffers();
	}, "tone_mapping_pass", renderer_context, get_render_basis_sort_order(), true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
display_mapping_renderer::display_mapping_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	resize_buffer(0, 0);

	pipeline_state_manager::instance()->generate_pipeline_state<display_mapping_pipeline_state>("display_mapping", &display_mapping_pipeline_states);
	display_mapping_pipeline_states->set_domain(pipeline_state_base::domains::system);

	constant_buffer = create_constant_buffer(sizeof(display_mapping_constants));
}

display_mapping_renderer::~display_mapping_renderer()
{
}

void display_mapping_renderer::render(renderer_context& renderer_context)
{
	job_executor.execute([this](graphics_context& context)
	{
		//	OETF変換
		const math::float3x3 matrix = color_space::get_gamut_matrix(engine_config_data::instance()->get_active_rendering_gamut(), color_space::gamuts::rec709);
		display_mapping_constants constants{};
		for (int row = 0; row < 3; ++row)
			constants.rendering_to_output[row] = { matrix.m[row][0], matrix.m[row][1], matrix.m[row][2], 0.0f };
		constants.peak_nits = sdr_reference_white_nits;
		constants.luminous_efficacy = static_cast<float>(light_utility::luminous_efficacy);
		constants.output_curve = static_cast<UINT>(output_curves::srgb);
		context.device_context->UpdateSubresource(constant_buffer.Get(), 0, nullptr, &constants, 0, 0);

		render_targets->output->activate(context.device_context);
		context.device_context->RSSetViewports(1, &viewport);
		context.scene_color_texture = render_targets->scene_color_texture;

		display_mapping_pipeline_states->begin(context);
		context.device_context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
		draw(context, 4);
		display_mapping_pipeline_states->end(context);

		render_targets->output->deactivate(context.device_context);
	}, "display_mapping_pass", renderer_context, get_render_basis_sort_order());
}

void display_mapping_renderer::resize_buffer(int width, int height)
{
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(render_targets->output->get_size().x);
	viewport.Height = static_cast<float>(render_targets->output->get_size().y);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}
