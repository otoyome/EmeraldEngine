#pragma once

#include "renderer.h"
#include "Graphics/Core/render_pass.h"
#include "scene_render_targets.h"
#include "Graphics/Geometry/geometric_primitive.h"

#include "collision/frustum_culling.h"
#include "Graphics/Culling/meshlet_culling_system.h"
#include "Graphics/Culling/hiz_buffer.h"

namespace scene_renderer_commons
{
	//	ÉrÉÖÅ[ãÛä‘ÇÃê[ìxÇÉJÉÅÉâÇÃéÀâeê[ìxÇ…ïœä∑Ç∑ÇÈ(MATH_REVERSE_DEPTHÇÃê›íËÇ…è]Ç§)
	float convert_view_depth_to_projection_depth(float view_depth, float near_clip, float far_clip);

	void draw(const graphics_context& context,
			  graphics_pipeline_state_ptr pipeline_state,
			  UINT vertex_count,
			  D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
			  UINT vertex_offset = 0);

	void draw_indexed(const graphics_context& context,
					  float mesh_length,
					  const mesh_draw_item& buffer_data,
					  UINT primitive_index,
					  graphics_pipeline_state_ptr pipeline_state,
					  const collision::frustum_culling* meshlet_frustum_culling = nullptr);

	void draw_geometric_primitive(const graphics_context& context,
					  const geometric_primitive& primitive);

	void model_render(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const std::vector<float>& mesh_lengths,
					  material_resource::surface_paths surface_path,
					  const collision::frustum_culling* meshlet_frustum_culling = nullptr);

	void model_render(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const collision::frustum_culling& frustum_culling,
					  material_resource::surface_paths surface_path);

	void model_render_shadow_batched(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const collision::frustum_culling& frustum_culling,
					  material_resource::surface_paths surface_path);

	void model_render_shadow_batched(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const math::float3& sphere_center,
					  float sphere_radius,
					  material_resource::surface_paths surface_path);

	void model_render_indirect(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const std::vector<float>& mesh_lengths,
					  material_resource::surface_paths surface_path,
					  meshlet_culling_system& meshlet_culling);

	void particle_render(graphics_context& context,
						 const std::vector<std::size_t>& mesh_indices,
						 material_resource::particle_paths particle_path);
}	//	namespace scene_renderer_commons
