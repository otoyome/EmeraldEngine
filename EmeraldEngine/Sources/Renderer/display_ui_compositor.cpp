#include "Renderer/display_ui_compositor.h"

#include "Graphics/Core/render_data.h"
#include "Graphics/Pipeline/global_shader.h"

namespace
{
	//	グローバルシェーダー
	global_shader<vertex_shader> global_fullscreen_quad_vs("Engine/EngineShader/FullscreenQuad/fullscreen_quad_vs.hlsl");
	global_shader<pixel_shader> global_ui_composite_ps("Engine/EngineShader/Postprocess/DisplayMapping/ui_composite_ps.hlsl");

	//	定数バッファ
	struct ui_composite_constants
	{
		float sdr_white_nits;	//	SDR の白の輝度[nit]
		UINT output_curve;		//	出力のカーブ(0:sRGB 1:ST.2084)
		float dummy[2];
	};

	static constexpr UINT ui_composite_cbv_index = 0;
	static constexpr UINT ui_map_index = 0;
}

display_ui_compositor::display_ui_compositor()
{
	render_data& data = *render_data::instance();
	render_state_cache& states = data.get_state_cache();

	//	定数バッファ
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.ByteWidth = sizeof(ui_composite_constants);
		HRESULT hr = data.get_device()->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	ブレンドステート
	{
		//	乗算済みアルファ
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blend_state = states.get_blend_state(blend_desc);
	}

	//	深度ステンシルステート
	{
		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
		depth_stencil_desc.DepthEnable = FALSE;
		depth_stencil_state = states.get_depth_stencil_state(depth_stencil_desc);
	}

	//	ラスタライザーステート
	{
		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_state = states.get_rasterizer_state(rasterizer_desc);
	}
}

display_ui_compositor::~display_ui_compositor()
{
}

void display_ui_compositor::composite()
{
	render_data& data = *render_data::instance();
	ID3D11RenderTargetView* back_buffer_target = data.get_back_buffer_target();
	ID3D11RenderTargetView* ui_texture_target = data.get_ui_texture_target();
	ID3D11ShaderResourceView* ui_texture_view = data.get_ui_texture_view();
	if (!back_buffer_target || !ui_texture_target || !ui_texture_view)
		return;

	ID3D11DeviceContext* context = data.get_immediate_context().Get();
	const std::shared_ptr<vertex_shader> vs = global_fullscreen_quad_vs.get();
	const std::shared_ptr<pixel_shader> ps = global_ui_composite_ps.get();
	if (vs && ps)
	{
		//	定数バッファ更新
		ui_composite_constants constants{};
		constants.sdr_white_nits = data.get_display_color_space_data().sdr_white_nits;
		constants.output_curve = data.is_hdr_output() ? 1u : 0u;
		context->UpdateSubresource(constant_buffer.Get(), 0, nullptr, &constants, 0, 0);

		//	描画先とビューポート
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		back_buffer_target->GetResource(resource.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
		D3D11_TEXTURE2D_DESC desc{};
		if (SUCCEEDED(resource.As(&back_buffer)))
			back_buffer->GetDesc(&desc);
		D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(desc.Width), static_cast<float>(desc.Height), 0.0f, 1.0f };
		context->OMSetRenderTargets(1, &back_buffer_target, nullptr);
		context->RSSetViewports(1, &viewport);

		//	シェーダー設定
		context->VSSetShader(vs->get_shader().Get(), nullptr, 0);
		context->HSSetShader(nullptr, nullptr, 0);
		context->DSSetShader(nullptr, nullptr, 0);
		context->GSSetShader(nullptr, nullptr, 0);
		context->PSSetShader(ps->get_shader().Get(), nullptr, 0);
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		//	ステート設定
		context->OMSetBlendState(blend_state.Get(), nullptr, 0xFFFFFFFF);
		context->OMSetDepthStencilState(depth_stencil_state.Get(), 0);
		context->RSSetState(rasterizer_state.Get());

		//	UI のテクスチャ設定
		context->PSSetConstantBuffers(ui_composite_cbv_index, 1, constant_buffer.GetAddressOf());
		context->PSSetShaderResources(ui_map_index, 1, &ui_texture_view);
		context->Draw(4, 0);

		ID3D11ShaderResourceView* null_view = nullptr;
		context->PSSetShaderResources(ui_map_index, 1, &null_view);
		context->OMSetRenderTargets(0, nullptr, nullptr);
	}

	//	クリア
	const FLOAT clear_color[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
	context->ClearRenderTargetView(ui_texture_target, clear_color);
}
