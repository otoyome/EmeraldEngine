#pragma once

#include "renderer.h"
#include "render_pass.h"
#include "scene_render_targets.h"

//	UIƒpƒX
class ui_renderer : public renderer
{
	static constexpr size_t vertex_max = 0xFFFF;
	static constexpr size_t index_max = 0x000FFFFF;

public:
	renderer_domain get_domain() const override { return renderer_domain::ui_rendering; }

public:
	ui_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~ui_renderer() override;

	void render(renderer_context& context) override;
	void resize_buffer(int width, int height) override;

private:
	struct sprite_buffer_views
	{
		buffer_view views[3];
	};

	void sprite_render(graphics_context& context, const std::vector<sprite_buffer_views>& vbvs, const std::vector<buffer_view>& ibvs);

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;

	std::unordered_map<semantic, buffer_view> vertex_buffer_views;
	buffer_view index_buffer_view;

	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_constant_buffer;
};
