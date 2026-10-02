#include "Renderer/scene_renderer_forward.h"

#include "model_instance.h"

#include "custom_pipeline_state.h"
#include "PipelineState/deferred_lighting_pipeline_state.h"
#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "PipelineState/debug_draw_line_pipeline_state.h"

#include "PipelineState/post_process_pipeline_state.h"

#include "Renderer/scene_renderer_common.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
before_dof_forward_renderer::before_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture);
	if (texture)
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}
}

before_dof_forward_renderer::~before_dof_forward_renderer()
{
}

void before_dof_forward_renderer::render(renderer_context& renderer_context)
{
	static constexpr int  forward_index = static_cast<int>(material::translucency_paths::before_dof);
	if (renderer_context.render_mesh_indices->forward[forward_index].empty() &&
		renderer_context.render_particle_indices->forward[forward_index].empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		scene_renderer_commons::model_render(context,
					 renderer_context.render_mesh_indices->forward[forward_index],
					 renderer_context.render_mesh_indices->lengths,
					 material::surface_paths::forward_pass);
		scene_renderer_commons::particle_render(context,
					 renderer_context.render_particle_indices->forward[forward_index],
					 material::surface_paths::forward_pass);

		rendering_pass.deactivate(context.device_context);
	}, "forward_pass : before_dof", renderer_context, get_render_basis_sort_order());
}

void before_dof_forward_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
after_dof_forward_renderer::after_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture);
	if (texture)
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}
}

after_dof_forward_renderer::~after_dof_forward_renderer()
{
}

void after_dof_forward_renderer::render(renderer_context& renderer_context)
{
	static constexpr int  forward_index = static_cast<int>(material::translucency_paths::after_dof);
	if (renderer_context.render_mesh_indices->forward[forward_index].empty() &&
		renderer_context.render_particle_indices->forward[forward_index].empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		scene_renderer_commons::model_render(context,
					 renderer_context.render_mesh_indices->forward[forward_index],
					 renderer_context.render_mesh_indices->lengths,
					material::surface_paths::forward_pass);
		scene_renderer_commons::particle_render(context,
					 renderer_context.render_particle_indices->forward[forward_index],
					 material::surface_paths::forward_pass);

		rendering_pass.deactivate(context.device_context);
	}, "forward_pass : after_dof", renderer_context, get_render_basis_sort_order());
}

void after_dof_forward_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
after_motion_blur_forward_renderer::after_motion_blur_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	//	バッファ生成
	texture2d_ptr texture = std::static_pointer_cast<texture2d>(render_targets->scene_color_texture);
	if (texture)
	{
		resize_buffer(texture->get_desc().Width, texture->get_desc().Height);
	}
}

after_motion_blur_forward_renderer::~after_motion_blur_forward_renderer()
{
}

void after_motion_blur_forward_renderer::render(renderer_context& renderer_context)
{
	static constexpr int  forward_index = static_cast<int>(material::translucency_paths::after_motion_blur);
	if (renderer_context.render_mesh_indices->forward[forward_index].empty() &&
		renderer_context.render_particle_indices->forward[forward_index].empty())
		return;

	job_executor.execute([this, renderer_context](graphics_context& context)
	{
		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	モデル類描画
		scene_renderer_commons::model_render(context,
					 renderer_context.render_mesh_indices->forward[forward_index],
					 renderer_context.render_mesh_indices->lengths,
					 material::surface_paths::forward_pass);
		scene_renderer_commons::particle_render(context,
					 renderer_context.render_particle_indices->forward[forward_index],
					 material::surface_paths::forward_pass);

		rendering_pass.deactivate(context.device_context);
	}, "forward_pass: after_motion_blur", renderer_context, get_render_basis_sort_order());
}

void after_motion_blur_forward_renderer::resize_buffer(int width, int height)
{
	if (viewport.Width == static_cast<float>(width) && viewport.Height == static_cast<float>(height))
		return;

	//	カラーパス生成
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_targets->scene_color_texture);
	auto& rtv = rendering_pass.render_targets.back();
	rtv.clear_color = false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	rendering_pass.depth_stencil = depth_stencil_view(render_targets->scene_depth_stencil_texture, &depth_stencil_view_desc);
	rendering_pass.depth_stencil.clear_depth = false;

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}
