#include "PipelineState/skybox_pipeline_state.h"

skybox_pipeline_state::skybox_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/sky_box_vs.cso";
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/sky_box_gbuffer_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(ps, ps_desc);

	//	定数バッファの生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		{
			buffer_desc.ByteWidth = sizeof(common_pipeline_states::deferred_light_probe_constants);
			hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, lcbv.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}

	//	深度ステンシルステート
	{
		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
		depth_stencil_desc.DepthEnable = TRUE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = common_pipeline_states::depth_comparison_func;
		depth_stencil_states = render_data::instance()->get_state_cache().get_depth_stencil_state(depth_stencil_desc);
	}

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blend_states = render_data::instance()->get_state_cache().get_blend_state(blend_desc);
	}

	// ラスタライザーステートの生成
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;
		rasterizer_states = render_data::instance()->get_state_cache().get_rasterizer_state(rasterizer_desc);
	}
}

skybox_pipeline_state::~skybox_pipeline_state()
{
}

void skybox_pipeline_state::begin(const graphics_context& context)
{
	//	シェーダー設定
	context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
	context.device_context->HSSetShader(nullptr, nullptr, 0);
	context.device_context->DSSetShader(nullptr, nullptr, 0);
	context.device_context->GSSetShader(nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps->get_shader().Get(), nullptr, 0);
	context.device_context->IASetInputLayout(nullptr);
	context.device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//	ステート設定
	context.device_context->OMSetBlendState(blend_states.Get(), nullptr, 0xFFFFFFFF);
	context.device_context->OMSetDepthStencilState(depth_stencil_states.Get(), 0);
	context.device_context->RSSetState(rasterizer_states.Get());

	//	シーン定数バッファ設定
	context.device_context->VSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());

	//	IBLテクスチャ処理
	static constexpr UINT SkyboxIndex = 0;
	static constexpr UINT SkyboxSamplerIndex = 0;
	if (context.specluar_ibl_texture)
		context.device_context->PSSetShaderResources(SkyboxIndex, 1, context.specluar_ibl_texture->get_shader_resource_view().GetAddressOf());
	context.device_context->PSSetSamplers(SkyboxSamplerIndex, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).GetAddressOf());

	//	ライト情報設定
	context.device_context->UpdateSubresource(lcbv.Get(), 0, 0, &context.deferred_light_probe_constant, 0, 0);
	context.device_context->PSSetConstantBuffers(common_pipeline_states::light_probe_cbv_index, 1, lcbv.GetAddressOf());
}

void skybox_pipeline_state::update_parameters(const graphics_context& context, const material_instance& material, const buffer_datas* buffer_data)
{
}

void skybox_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}
