#pragma once

#include "renderer.h"
#include "render_pass.h"
#include "scene_render_targets.h"
#include "geometric_primitive.h"
#include "material_instance.h"

#include "PipelineState/deferred_decal_pipeline_state.h"

#include "collision/frustum_culling.h"
#include "graphics/shadow_atlas.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	深度プリパス
class depth_prepass_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::depth_prepass; }

public:
	depth_prepass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~depth_prepass_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	GBuffer
class gbuffer_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::g_buffer_pass; }

public:
	gbuffer_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~gbuffer_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;

	graphics_pipeline_state_ptr skybox_pipeline_states;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	GBufferデカール
class gbuffer_decal_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::g_buffer_decal_pass; }

public:
	gbuffer_decal_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~gbuffer_decal_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass normal_depth_copy_rendering_pass;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;

	std::unique_ptr<geometric_cube> cube;
	Microsoft::WRL::ComPtr<ID3D11Buffer> decal_primitive_primitive_buffer;
	graphics_pipeline_state_ptr stencil_write_pipeline_state;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	ディファードライティング
class deffered_lighting_renderer : public renderer
{
	static constexpr UINT directional_shadow_map_count = 6;

public:
	renderer_domain get_domain() const override { return renderer_domain::deffered_lighting_pass; }

public:
	deffered_lighting_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~deffered_lighting_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	void render_indirect(renderer_context& context);
	void render_clustered(renderer_context& context);
	void render_directional(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context);
	void render_directional_shadow_map(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context);
	void render_point(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context, bool use_atlas = false);
	void render_point_shadow_map(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context, int atlas_slice = -1, bool render_shadow = true);
	void render_spot(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context, bool use_atlas = false);
	void render_spot_shadow_map(const light_render_data& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, renderer_context& context, int atlas_slice = -1, bool render_shadow = true);

	void resize_shadow_buffer(int width, int height);

	void begin_shadow_target(graphics_context& context, ID3D11DepthStencilView* atlas_depth_stencil_view);
	void end_shadow_target(graphics_context& context, ID3D11DepthStencilView* atlas_depth_stencil_view);

private:
	std::shared_ptr<scene_render_targets> render_targets;
	UINT register_index;
	render_pass lighting_pass;
	D3D11_VIEWPORT lighting_pass_viewport;

	render_pass shadow_map_pass;
	D3D11_VIEWPORT shadow_map_pass_viewport;

	shadow_atlas atlas;
	std::shared_ptr<material_resource> deferred_lighting_material;
	material_instance deferred_lighting_material_instance;
};

#if defined(USE_IMGUI)
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	デバッグ描画パス
class debug_draw_pass_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::debug_draw; }

public:
	debug_draw_pass_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~debug_draw_pass_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;

	graphics_pipeline_state_ptr debug_draw_line_pipeline_states[magic_enum::enum_count<debug_draw_primitive_container::line_buffer_id>()];
};
#endif // defined(USE_IMGUI)
