#pragma once

#include "renderer.h"
#include "scene_render_targets.h"

class occlusion_culling_system;

//	@tool occlusion_culling_system::update を呼ぶだけの薄い renderer
//	@tool 実処理は occlusion_culling_system 側。ここはジョブ登録・ソート順の付与・解像度／深度 SRV の橋渡しのみ
class occlusion_culling_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::occlusion_culling_pass; }

public:
	occlusion_culling_renderer(render_job_registrar job_executor, occlusion_culling_system* system, std::shared_ptr<scene_render_targets> render_targets);
	~occlusion_culling_renderer() override;

	void render(renderer_context& context) override;
	void resize_buffer(int width, int height) override;

private:
	occlusion_culling_system* system = nullptr;
	std::shared_ptr<scene_render_targets> render_targets;
};
