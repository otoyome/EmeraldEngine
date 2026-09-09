#pragma once

#include "renderer.h"
#include "scene_render_targets.h"

class occlusion_culling_system;

//	オクルージョンカリング呼び出し用レンダラー
class occlusion_culling_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::occlusion_culling_pass; }

public:
	occlusion_culling_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~occlusion_culling_renderer() override;

	void render(renderer_context& context) override;
	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
};
