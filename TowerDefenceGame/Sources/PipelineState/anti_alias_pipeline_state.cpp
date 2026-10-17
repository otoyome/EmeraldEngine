#include "PipelineState/anti_alias_pipeline_state.h"

#include "subpixel_morphological_anti_aliasing/textures/AreaTex.h"
#include "subpixel_morphological_anti_aliasing/textures/SearchTex.h"

namespace smaa
{
	//	プリセット設定にしているので必要であればこれを作成して対応する事
	struct constants
	{
		float smaa_threshold = 0.05f;	//	エッジ検出閾値指定
		UINT smaa_max_search_steps = 32;	//	水平/垂直パターン検索で実行される最大ステップ数
		UINT smaa_max_search_steps_diagonal = 16;	//	対角パターン検索の最大ステップ数
		UINT smaa_corner_rounding = 25;	//	角の丸みを調整
		float smaa_local_contrast_adaptation_factor = 2.0f;	//	不要な交差エッジを除去するための係数
		math::uint2 smaa_dummy;
		bool smaa_draw_edge = false;	//	デバッグ用の適応エッジ表示判定
	};
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
fxaa_pipeline_state::fxaa_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(shaders.vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/fast_approximate_anti_aliasing_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(shaders.ps, ps_desc);

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
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
	layout.add_shader_resource(shader_stage::pixel, common_pipeline_states::scene_color_map_srv_index, common_pipeline_states::scene_color_map_hash_key);
	layout.add_sampler(shader_stage::pixel, 0, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR));
}

fxaa_pipeline_state::~fxaa_pipeline_state()
{
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
smaa_edge_detection_pipeline_state::smaa_edge_detection_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/smaa_edge_detection_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(shaders.vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/smaa_edge_detection_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(shaders.ps, ps_desc);

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
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
	layout.add_shader_resource(shader_stage::pixel, common_pipeline_states::scene_color_map_srv_index, common_pipeline_states::scene_color_map_hash_key);
	layout.add_sampler(shader_stage::pixel, 0, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT));
	layout.add_sampler(shader_stage::pixel, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR));
}

smaa_edge_detection_pipeline_state::~smaa_edge_detection_pipeline_state()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
smaa_blending_weight_calculation_pipeline_state::smaa_blending_weight_calculation_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/smaa_blending_weight_calculation_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(shaders.vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/smaa_blending_weight_calculation_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(shaders.ps, ps_desc);

	//	パラメーターテクスチャ生成
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> dummy_resource;
		//	Area
		{
			texture2d_desc.Width = AREATEX_WIDTH;
			texture2d_desc.Height = AREATEX_HEIGHT;
			texture2d_desc.Format = DXGI_FORMAT_R8G8_UNORM;

			D3D11_SUBRESOURCE_DATA initialize_data = {};
			initialize_data.pSysMem = areaTexBytes;
			initialize_data.SysMemPitch = AREATEX_PITCH;
			texture_manager::instance()->generate_texture2d(area_texture, "AreaTex", texture2d_desc, initialize_data);
		}
		//	Search
		{
			texture2d_desc.Width = SEARCHTEX_WIDTH;
			texture2d_desc.Height = SEARCHTEX_HEIGHT;
			texture2d_desc.Format = DXGI_FORMAT_R8_UNORM;

			D3D11_SUBRESOURCE_DATA initialize_data = {};
			initialize_data.pSysMem = searchTexBytes;
			initialize_data.SysMemPitch = SEARCHTEX_PITCH;
			texture_manager::instance()->generate_texture2d(search_texture, "SearchTex", texture2d_desc, initialize_data);
		}
	}

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
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
	layout.add_shader_resource(shader_stage::pixel, common_pipeline_states::scene_color_map_srv_index, common_pipeline_states::scene_color_map_hash_key);
	layout.add_shader_resource(shader_stage::pixel, 1, area_texture);
	layout.add_shader_resource(shader_stage::pixel, 2, search_texture);
	layout.add_sampler(shader_stage::pixel, 0, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT));
	layout.add_sampler(shader_stage::pixel, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR));
}

smaa_blending_weight_calculation_pipeline_state::~smaa_blending_weight_calculation_pipeline_state()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
smaa_neighborhood_blending_pipeline_state::smaa_neighborhood_blending_pipeline_state()
: graphics_pipeline_state()
{
	set_domain(pipeline_state_base::domains::system);
	HRESULT hr{ S_OK };

	load_shader_cso vs_desc;
	vs_desc.file_path = "Assets/CSO/smaa_neighborhood_blending_vs.cso";
	vs_desc.input_element_desc;
	shader_manager::instance()->load_shader<vertex_shader>(shaders.vs, vs_desc);

	load_shader_cso ps_desc;
	ps_desc.file_path = "Assets/CSO/smaa_neighborhood_blending_ps.cso";
	shader_manager::instance()->load_shader<pixel_shader>(shaders.ps, ps_desc);

	// ブレンドステートの生成
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
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
	layout.add_shader_resource(shader_stage::pixel, common_pipeline_states::scene_color_map_srv_index, common_pipeline_states::scene_color_map_hash_key);
	layout.add_shader_resource(shader_stage::pixel, common_pipeline_states::work_texture_srv_start + 0, common_pipeline_states::work_texture_hash_keys[0]);
	layout.add_shader_resource(shader_stage::pixel, common_pipeline_states::work_texture_srv_start + 1, common_pipeline_states::work_texture_hash_keys[1]);
	layout.add_sampler(shader_stage::pixel, 0, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT));
	layout.add_sampler(shader_stage::pixel, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR));
}

smaa_neighborhood_blending_pipeline_state::~smaa_neighborhood_blending_pipeline_state()
{
}

