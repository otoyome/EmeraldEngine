#include "Renderer/occlusion_culling_renderer.h"

#include "graphics/occlusion_culling_system.h"
#include "graphics/hiz_buffer.h"
#include "graphics/meshlet_culling_system.h"
#include "Graphics/camera.h"
#include "Graphics/render_data.h"

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
	if (!render_targets || !context.current_view_state)
		return;

	const auto& state = *context.current_view_state;
	auto occlusion = state.get_occlusion_culling();
	auto hiz = state.get_hiz_buffer();
	auto meshlet_culling = state.get_meshlet_culling();
	if (!occlusion || !hiz || !meshlet_culling)
		return;

	auto depth_texture = std::static_pointer_cast<texture2d>(render_targets->scene_depth_stencil_texture);
	if (depth_texture)
		hiz->resize_buffer(static_cast<int>(depth_texture->get_desc().Width), static_cast<int>(depth_texture->get_desc().Height));

#if defined(USE_IMGUI)
	bool freeze_culling = state.is_freeze_culling();
#endif	//	defined(USE_IMGUI)

	//	メッシュレットカリング情報
	std::vector<std::size_t> visible_draw_indices;
	if (context.render_mesh_indices)
	{
		visible_draw_indices.reserve(context.render_mesh_indices->opaque.size() + context.render_mesh_indices->mask.size());
		visible_draw_indices.insert(visible_draw_indices.end(), context.render_mesh_indices->opaque.begin(), context.render_mesh_indices->opaque.end());
		visible_draw_indices.insert(visible_draw_indices.end(), context.render_mesh_indices->mask.begin(), context.render_mesh_indices->mask.end());
	}

	auto render_mesh_indices = context.render_mesh_indices;
	job_executor.execute([this, occlusion, hiz, meshlet_culling, visible_draw_indices, render_mesh_indices
#if defined(USE_IMGUI)
		, freeze_culling
#endif	//	defined(USE_IMGUI)
		](graphics_context& context)
	{
		context.debug_begin_event(L"occlusion_culling");

		context.scene_depth_texture = render_targets->scene_depth_stencil_texture;

		//	階層Zバッファ構築
		hiz->build(context);

		//	オクルージョンカリング実行
		occlusion->update(context, *hiz);

		//	メッシュレットカリング実行
#if defined(USE_IMGUI)
		if (!freeze_culling && render_mesh_indices)
#else	//	defined(USE_IMGUI)
		if (render_mesh_indices)
#endif	//	defined(USE_IMGUI)
		{
			meshlet_culling->build_and_cull(context, visible_draw_indices, render_mesh_indices->lengths, *hiz);
		}

		context.scene_depth_texture = nullptr;

		context.debug_end_event();
	}, "occlusion_culling_pass", context, get_render_basis_sort_order(), true);
}

void occlusion_culling_renderer::resize_buffer(int width, int height)
{
}
