#include "PipelineState/skybox_pipeline_state.h"

skybox_pipeline_state::skybox_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/sky_box_vs.cso";
	shader_manager::instance()->load_shader<vertex_shader>(shaders.vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/sky_box_gbuffer_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(shaders.ps, ps_desc);


	//	深度ステンシルステート
	{
		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
		depth_stencil_desc.DepthEnable = TRUE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = common_pipeline_states::depth_comparison_func;
		hr = render_data::instance()->get_device()->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
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
		hr = render_data::instance()->get_device()->CreateBlendState(&blend_desc, blend_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
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
		hr = render_data::instance()->get_device()->CreateRasterizerState(&rasterizer_desc, rasterizer_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	@tool 描画設定と割り当て配置（描画時は command_recorder がこれを解釈して設定する）
	primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	layout.add_constant_buffer(shader_stage::vertex, common_pipeline_states::scene_cbv_index, common_pipeline_states::scene_constant_buffer_hash_key);
	layout.add_constant_buffer(shader_stage::pixel, common_pipeline_states::scene_cbv_index, common_pipeline_states::scene_constant_buffer_hash_key);
	layout.add_shader_resource(shader_stage::pixel, common_pipeline_states::deferred_ibl_srv_start + 1, common_pipeline_states::deferred_specular_pmrem_hash_key);
	layout.add_sampler(shader_stage::pixel, 0, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR));
	layout.add_constant_buffer(shader_stage::pixel, common_pipeline_states::light_probe_cbv_index, common_pipeline_states::deferred_light_probe_constant_buffer_hash_key);
}

skybox_pipeline_state::~skybox_pipeline_state()
{
}

