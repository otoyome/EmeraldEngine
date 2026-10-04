#pragma once

#include "renderer.h"
#include "render_pass.h"
#include "scene_render_targets.h"
#include "geometric_primitive.h"

#include "collision/frustum_culling.h"
#include "graphics/meshlet_culling_system.h"
#include "graphics/hiz_buffer.h"

namespace scene_renderer_commons
{
	float convert_linear_depth_to_projection_depth(float depth, float z_buffer_parameteres_y);

	void draw(const graphics_context& context,
			  graphics_pipeline_state_ptr pipeline_state,
			  UINT vertex_count,
			  D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
			  UINT vertex_offset = 0);

	void draw_indexed(const graphics_context& context,
					  float mesh_length,
					  const buffer_datas& buffer_data,
					  UINT primitive_index,
					  graphics_pipeline_state_ptr pipeline_state,
					  const collision::frustum_culling* meshlet_frustum_culling = nullptr);

	void draw_geometric_primitive(const graphics_context& context,
					  const geometric_primitive& primitive);

	void model_render(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const std::vector<float>& mesh_lengths,
					  material::surface_paths surface_path,
					  const collision::frustum_culling* meshlet_frustum_culling = nullptr);

	void model_render(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const collision::frustum_culling& frustum_culling,
					  material::surface_paths surface_path);

	void model_render_shadow_batched(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const collision::frustum_culling& frustum_culling,
					  material::surface_paths surface_path);

	void model_render_shadow_batched(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const math::float3& sphere_center,
					  float sphere_radius,
					  material::surface_paths surface_path);

	void model_render_indirect(graphics_context& context,
					  const std::vector<std::size_t>& mesh_indices,
					  const std::vector<float>& mesh_lengths,
					  material::surface_paths surface_path,
					  meshlet_culling_system& meshlet_culling);

	void particle_render(graphics_context& context,
						 const std::vector<std::size_t>& mesh_indices,
						 material::surface_paths surface_path);
}	//	namespace scene_renderer_commons
