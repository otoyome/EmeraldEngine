#include "PipelineState/deferred_decal_pipeline_state.h"

namespace
{
	//	定数バッファインデックス
	static constexpr UINT TransformBufferIndex = 1;
	static constexpr UINT GBufferDecalBufferIndex = 2;
	struct gbuffer_decal_constant_buffer
	{
		math::float4x4 decal_transform;
		math::float4x4 decal_inverse_transform;
		math::float4 decal_direction;
	};

	//	テクスチャ
	static constexpr UINT BaseMapIndex = 10;
	static constexpr UINT NormalDepthMapIndex = 11;
	static constexpr UINT UniqueMapIndex = 12;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
deferred_decal_stencil_write_pipeline_state::deferred_decal_stencil_write_pipeline_state()
: graphics_pipeline_state()
{
	load_shader_file vs_desc{};
	vs_desc.file_path = "Engine/EngineShader/DeferredDecal/gbuffer_decal_primitive_vs.hlsl";
	shader_manager::instance()->load_shader<vertex_shader>(vs, vs_desc);

	//	深度ステンシルステート
	{
		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
		depth_stencil_desc.DepthEnable = TRUE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = common_pipeline_states::depth_comparison_func;
		depth_stencil_desc.StencilEnable = TRUE;
		depth_stencil_desc.StencilReadMask = 0xFF;
		depth_stencil_desc.StencilWriteMask = 0xFF;
		depth_stencil_desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;		//	ステンシルステートは常に合格
		depth_stencil_desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;	//	合格したら1を入れる
		depth_stencil_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		depth_stencil_desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		depth_stencil_desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;	//	ステンシルステートは指定
		depth_stencil_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		depth_stencil_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		depth_stencil_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
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
		rasterizer_desc.CullMode = D3D11_CULL_FRONT;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = TRUE;
		rasterizer_states = render_data::instance()->get_state_cache().get_rasterizer_state(rasterizer_desc);
	}
}

deferred_decal_stencil_write_pipeline_state::~deferred_decal_stencil_write_pipeline_state()
{
}

void deferred_decal_stencil_write_pipeline_state::begin(const graphics_context& context)
{
	//	シェーダー設定
	context.device_context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
	context.device_context->HSSetShader(nullptr, nullptr, 0);
	context.device_context->DSSetShader(nullptr, nullptr, 0);
	context.device_context->GSSetShader(nullptr, nullptr, 0);
	context.device_context->PSSetShader(nullptr, nullptr, 0);
	context.device_context->IASetInputLayout(nullptr);
	context.device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//	ステート設定
	context.device_context->OMSetBlendState(blend_states.Get(), nullptr, 0xFFFFFFFF);
	context.device_context->OMSetDepthStencilState(depth_stencil_states.Get(), common_pipeline_states::decal_stencil_ref);
	context.device_context->RSSetState(rasterizer_states.Get());

	//	シーン定数バッファ設定
	context.device_context->VSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
}

void deferred_decal_stencil_write_pipeline_state::update_parameters(const graphics_context& context, const material_instance& material, const buffer_datas* buffer_data)
{
	if (!buffer_data)
		return;
	context.device_context->VSSetConstantBuffers(common_pipeline_states::primitive_cbv_index, 1, buffer_data->primitive_buffer.GetAddressOf());
}

void deferred_decal_stencil_write_pipeline_state::end(const graphics_context& context)
{
	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();
}
