#pragma once

#include "common_pipeline_states.h"

//	ディスプレイマッピング(描画色域の放射輝度 > ディスプレイの信号)
class display_mapping_pipeline_state : public graphics_pipeline_state
{
public:
	display_mapping_pipeline_state();
	~display_mapping_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void update_parameters(const graphics_context& context, const material_instance& material, const buffer_datas* buffer_data) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_states;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;
};

//	リニア色空間>sRGB色空間に変換
class convert_srgb_to_linear_pipeline_state : public graphics_pipeline_state
{
public:
	convert_srgb_to_linear_pipeline_state();
	~convert_srgb_to_linear_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void update_parameters(const graphics_context& context, const material_instance& material, const buffer_datas* buffer_data) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_states;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;
};






