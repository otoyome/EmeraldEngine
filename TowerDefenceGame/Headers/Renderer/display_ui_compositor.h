#pragma once

//	UIとImGuiをバックバッファへ合成
class display_ui_compositor
{
public:
	display_ui_compositor();
	~display_ui_compositor();

	display_ui_compositor(const display_ui_compositor&) = delete;
	display_ui_compositor& operator=(const display_ui_compositor&) = delete;
	display_ui_compositor(display_ui_compositor&&) noexcept = delete;
	display_ui_compositor& operator=(display_ui_compositor&&) noexcept = delete;

	void composite();

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_state;
};
