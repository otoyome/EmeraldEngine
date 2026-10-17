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

	//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
	void register_input_element_stream_vertex(std::vector<D3D11_INPUT_ELEMENT_DESC>& descs, LPCSTR semantic_name, UINT semantic_index, DXGI_FORMAT format, UINT& stream_index)
	{
		descs.emplace_back(D3D11_INPUT_ELEMENT_DESC{ semantic_name, semantic_index, format, stream_index, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
		++stream_index;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
deferred_decal_stencil_write_pipeline_state::deferred_decal_stencil_write_pipeline_state()
: graphics_pipeline_state()
{
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/gbuffer_decal_primitive_vs.cso";
	{
		UINT stream_index = 0;
		register_input_element_stream_vertex(vs_desc.input_element_desc, "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, stream_index);
		register_input_element_stream_vertex(vs_desc.input_element_desc, "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, stream_index);
	}
	shader_manager::instance()->load_shader<vertex_shader>(shaders.vs, vs_desc);

	//	セマンティックリスト作成
	semantic_list.clear();
	for (auto& element_desc : shaders.vs->get_input_element_desc())
	{
		semantic_list.emplace_back(std::make_pair(semantic(element_desc.SemanticName, element_desc.SemanticIndex), graphics_utility::get_byte_offset_from_format(element_desc.Format)));
	}

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
		rasterizer_desc.CullMode = D3D11_CULL_FRONT;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthBias = 0;
		rasterizer_desc.DepthBiasClamp = 0;
		rasterizer_desc.SlopeScaledDepthBias = 0;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = FALSE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = TRUE;
		hr = render_data::instance()->get_device()->CreateRasterizerState(&rasterizer_desc, rasterizer_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	@tool 描画設定と割り当て配置（描画時は command_recorder がこれを解釈して設定する）
	primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	stencil_ref = common_pipeline_states::decal_stencil_ref;
	layout.add_constant_buffer(shader_stage::vertex, common_pipeline_states::scene_cbv_index, common_pipeline_states::scene_constant_buffer_hash_key);
	layout.add_constant_buffer(shader_stage::vertex, common_pipeline_states::primitive_cbv_index, common_pipeline_states::primitive_constant_buffer_hash_key, binding_source::primitive);
}

deferred_decal_stencil_write_pipeline_state::~deferred_decal_stencil_write_pipeline_state()
{
}

