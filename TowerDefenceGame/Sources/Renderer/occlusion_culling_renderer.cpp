#include "Renderer/occlusion_culling_renderer.h"

#include "Renderer/occlusion_culling_system.h"
#include "render_data.h"

occlusion_culling_renderer::occlusion_culling_renderer(render_job_registrar job_executor, std::shared_ptr<occlusion_culling_system> system, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, system(system)
, render_targets(render_targets)
{
	if (render_targets)
	{
		texture2d_ptr texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture);
		if (texture)
			resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}
}

occlusion_culling_renderer::~occlusion_culling_renderer()
{
}

void occlusion_culling_renderer::render(renderer_context& context)
{
	if (!system || !render_targets)
		return;

	job_executor.execute([this](graphics_context& context)
	{
		context.debug_begin_event(L"occlusion_culling");

		context.scene_depth_texture = render_targets->scene_depth_stencil_texture;

		system->update(context);

		context.scene_depth_texture = nullptr;

		context.debug_end_event();
	}, "occlusion_culling_pass", context, get_render_basis_sort_order(), true);
}

void occlusion_culling_renderer::resize_buffer(int width, int height)
{
	if (system)
		system->resize_buffer(width, height);
}
