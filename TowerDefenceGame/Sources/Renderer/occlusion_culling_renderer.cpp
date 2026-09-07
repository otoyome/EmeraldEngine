#include "Renderer/occlusion_culling_renderer.h"

#include "Renderer/occlusion_culling_system.h"
#include "render_data.h"

occlusion_culling_renderer::occlusion_culling_renderer(render_job_registrar job_executor, occlusion_culling_system* system, std::shared_ptr<scene_render_targets> render_targets)
	: renderer(job_executor)
	, system(system)
	, render_targets(render_targets)
{
	//	@tool 生成時にシーンカラーの解像度で Hi-Z バッファを確保させる（resize_buffer は外部＝ここから呼ぶ）
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

	ID3D11ShaderResourceView* scene_depth_srv = nullptr;
	if (render_targets->scene_depth_stencil_texture)
		scene_depth_srv = render_targets->scene_depth_stencil_texture->get_shader_resource_view().Get();

	job_executor.execute([this, scene_depth_srv](graphics_context& context)
	{
		context.debug_begin_event(L"occlusion_culling");
		system->update(context, scene_depth_srv);
		context.debug_end_event();
	}, "occlusion_culling_pass", context, get_render_basis_sort_order(), true);
}

void occlusion_culling_renderer::resize_buffer(int width, int height)
{
	//	@tool Hi-Z／判定バッファの確保は occlusion_culling_system に委譲（外部から呼び出す口）
	if (system)
		system->resize_buffer(width, height);
}
