#include "Renderer/occlusion_culling_renderer.h"

#include "graphics/occlusion_culling_system.h"
#include "graphics/hiz_buffer.h"
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
	if (!render_targets || !context.occlusion_culling_system || !context.hiz_buffer)
		return;

	//	カメラ毎の occlusion_culling_system / hiz_buffer を使う
	auto occlusion = context.occlusion_culling_system;
	auto hiz = context.hiz_buffer;
	if (!occlusion || !hiz)
		return;

	//	Hi-Z バッファをシーン深度に合わせる
	auto depth_texture = std::static_pointer_cast<texture2d>(render_targets->scene_depth_stencil_texture);
	if (depth_texture)
		hiz->resize_buffer(static_cast<int>(depth_texture->get_desc().Width), static_cast<int>(depth_texture->get_desc().Height));

	float animation_test_distance = context.occlusion_animation_distance;

	job_executor.execute([this, occlusion, hiz, animation_test_distance](graphics_context& context)
	{
		context.debug_begin_event(L"occlusion_culling");

		context.scene_depth_texture = render_targets->scene_depth_stencil_texture;

		//	Hi-Z構築
		hiz->build(context);

		//	オクルージョンカリング実行
		occlusion->update(context, animation_test_distance, *hiz);

		context.scene_depth_texture = nullptr;

		context.debug_end_event();
	}, "occlusion_culling_pass", context, get_render_basis_sort_order(), true);
}

void occlusion_culling_renderer::resize_buffer(int width, int height)
{
}
