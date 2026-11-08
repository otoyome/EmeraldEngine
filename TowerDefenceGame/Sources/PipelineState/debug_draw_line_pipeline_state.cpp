#include "PipelineState/debug_draw_line_pipeline_state.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
debug_draw_line_pipeline_state::debug_draw_line_pipeline_state(bool depth_test)
: graphics_pipeline_state()
{
	load_shader_file vs_desc{};
	vs_desc.file_path = "Engine/EngineShader/GeometricPrimitive/debug_draw_line_vs.hlsl";
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	load_shader_file ps_desc{};
	ps_desc.file_path = "Engine/EngineShader/GeometricPrimitive/debug_draw_line_ps.hlsl";
	shader_manager::instance()->load_shader<pixel_shader>(ps, ps_desc);

	//	深度ステンシルステート
	{
		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
		depth_stencil_desc.DepthEnable = TRUE;
		depth_stencil_desc.DepthWriteMask = depth_test ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = depth_test ? common_pipeline_states::depth_comparison_func : D3D11_COMPARISON_ALWAYS;
		depth_stencil_states = render_data::instance()->get_state_cache().get_depth_stencil_state(depth_stencil_desc);
	}

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
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

debug_draw_line_pipeline_state::debug_draw_line_pipeline_state()
: debug_draw_line_pipeline_state(true)
{
}

debug_draw_line_pipeline_state::~debug_draw_line_pipeline_state()
{
}

void debug_draw_line_pipeline_state::begin(const graphics_context& context)
{
	//	シェーダー設定
	context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
	context.device_context->HSSetShader(nullptr, nullptr, 0);
	context.device_context->DSSetShader(nullptr, nullptr, 0);
	context.device_context->GSSetShader(nullptr, nullptr, 0);
	context.device_context->PSSetShader(ps->get_shader().Get(), nullptr, 0);
	context.device_context->IASetInputLayout(nullptr);
	context.device_context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);

	//	ステート設定
	context.device_context->OMSetBlendState(blend_states.Get(), nullptr, 0xFFFFFFFF);
	context.device_context->OMSetDepthStencilState(depth_stencil_states.Get(), 0);
	context.device_context->RSSetState(rasterizer_states.Get());

	//	シーン定数バッファ設定
	context.device_context->VSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
}

void debug_draw_line_pipeline_state::update_parameters(const graphics_context& context, const material_instance& material, const buffer_datas* buffer_data)
{
}

void debug_draw_line_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
debug_draw_line_disable_depth_test_pipeline_state::debug_draw_line_disable_depth_test_pipeline_state()
: debug_draw_line_pipeline_state(false)
{
}

debug_draw_line_disable_depth_test_pipeline_state::~debug_draw_line_disable_depth_test_pipeline_state()
{
}
