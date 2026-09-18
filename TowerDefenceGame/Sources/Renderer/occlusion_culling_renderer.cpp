#include "Renderer/occlusion_culling_renderer.h"

#include "graphics/occlusion_culling_system.h"
#include "camera.h"
#include "render_data.h"

occlusion_culling_renderer::occlusion_culling_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
{
}

occlusion_culling_renderer::~occlusion_culling_renderer()
{
}

void occlusion_culling_renderer::render(renderer_context& context)
{
	if (!render_targets || !context.occlusion_culling_system)
		return;

	//	カメラ毎の occlusion_culling_system を使う
	auto occlusion = context.occlusion_culling_system;
	if (!occlusion)
		return;

	//	Hi-Z バッファをシーン深度に合わせる
	auto depth_texture = std::static_pointer_cast<texture2d>(render_targets->scene_depth_stencil_texture);
	if (depth_texture)
		occlusion->resize_buffer(static_cast<int>(depth_texture->get_desc().Width), static_cast<int>(depth_texture->get_desc().Height));

	float animation_test_distance = context.occlusion_animation_distance;

	job_executor.execute([this, occlusion, animation_test_distance](graphics_context& context)
	{
		context.debug_begin_event(L"occlusion_culling");

		context.scene_depth_texture = render_targets->scene_depth_stencil_texture;

		occlusion->update(context, animation_test_distance);

		context.scene_depth_texture = nullptr;

		context.debug_end_event();
	}, "occlusion_culling_pass", context, get_render_basis_sort_order(), true);
}

void occlusion_culling_renderer::resize_buffer(int width, int height)
{
	//	Hi-Z はカメラ毎の occlusion_culling_system が update 内で再確保する
}
