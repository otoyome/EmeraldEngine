#pragma once

#include "Graphics/Pipeline/common_pipeline_states.h"

//	スカイボックスパス
class skybox_pipeline_state : public graphics_pipeline_state
{
public:
	skybox_pipeline_state();
	~skybox_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void update_parameters(const graphics_context& context, const material_instance& material, const mesh_draw_item* buffer_data) override;
	void end(const graphics_context& context) override;

private:
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<pixel_shader> ps;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_states;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states;

	Microsoft::WRL::ComPtr<ID3D11Buffer> lcbv;
};
