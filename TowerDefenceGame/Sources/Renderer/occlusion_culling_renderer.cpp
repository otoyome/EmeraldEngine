#include "Renderer/occlusion_culling_renderer.h"

#include "graphics/occlusion_culling_system.h"
#include "graphics/hiz_buffer.h"
#include "graphics/meshlet_culling_system.h"
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
	if (!render_targets || !context.occlusion_culling_system || !context.hiz_buffer || !context.meshlet_culling_system)
		return;

	//	カメラ毎の occlusion_culling_system / hiz_buffer / meshlet_culling_system を使う
	auto occlusion = context.occlusion_culling_system;
	auto hiz = context.hiz_buffer;
	auto meshlet_culling = context.meshlet_culling_system;
	if (!occlusion || !hiz || !meshlet_culling)
		return;

	//	Hi-Z バッファをシーン深度に合わせる
	auto depth_texture = std::static_pointer_cast<texture2d>(render_targets->scene_depth_stencil_texture);
	if (depth_texture)
		hiz->resize_buffer(static_cast<int>(depth_texture->get_desc().Width), static_cast<int>(depth_texture->get_desc().Height));

	float animation_test_distance = context.occlusion_animation_distance;

#if defined(USE_IMGUI)
	bool freeze_culling = context.freeze_culling;
#endif	//	defined(USE_IMGUI)

	//	メッシュレットカリング情報
	std::vector<std::size_t> visible_draw_indices;
	if (context.render_mesh_indices)
	{
		visible_draw_indices.reserve(context.render_mesh_indices->opaque.size() + context.render_mesh_indices->mask.size());
		visible_draw_indices.insert(visible_draw_indices.end(), context.render_mesh_indices->opaque.begin(), context.render_mesh_indices->opaque.end());
		visible_draw_indices.insert(visible_draw_indices.end(), context.render_mesh_indices->mask.begin(), context.render_mesh_indices->mask.end());
	}

	//	@tool グルーピングとバッファ確保は記録時に行い、後続パスが今フレームのバッファを参照できるようにする
#if defined(USE_IMGUI)
	bool execute_meshlet_culling = !freeze_culling && context.render_mesh_indices;
#else	//	defined(USE_IMGUI)
	bool execute_meshlet_culling = static_cast<bool>(context.render_mesh_indices);
#endif	//	defined(USE_IMGUI)
	if (execute_meshlet_culling)
	{
		meshlet_culling->prepare(*context.render_data, visible_draw_indices, context.render_mesh_indices->lengths, context.scene_constant.camera_clip_distance);
	}

	job_executor.execute([this, occlusion, hiz, meshlet_culling, animation_test_distance, execute_meshlet_culling](graphics_context& context)
	{
		context.debug_begin_event(L"occlusion_culling");

		context.scene_depth_texture = render_targets->scene_depth_stencil_texture;

		//	Hi-Z構築
		hiz->build(context);

		//	オクルージョンカリング実行
		occlusion->update(context, animation_test_distance, *hiz);

		if (execute_meshlet_culling)
		{
			meshlet_culling->cull(context, *hiz);
		}

		context.scene_depth_texture = nullptr;

		context.debug_end_event();
	}, "occlusion_culling_pass", context, get_render_basis_sort_order(), true);
}

void occlusion_culling_renderer::resize_buffer(int width, int height)
{
}
