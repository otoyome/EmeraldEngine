#include "Graphics/View/scene_view.h"
#include "Graphics/View/scene_view_state.h"

namespace
{
	//	render_origin からの相対座標系の視線行列生成
	math::matrixf make_view_matrix(const view_parameters& parameters, const math::vector3d& render_origin)
	{
		const math::vector3f relative_position((parameters.position - render_origin).to_float3());
		return math::matrixf::inverse_rigid_transformation(parameters.rotation.to_quaternion(), relative_position);
	}
}

scene_view scene_view::from_parameters(const view_parameters& parameters, const math::vector3d& render_origin)
{
	scene_view ret;
	ret.render_origin = render_origin;
	ret.relative_position = (parameters.position - ret.render_origin).to_float3();

	const math::matrixf rotation_matrix = math::matrixf::rotation_quaternion(parameters.rotation.to_quaternion());
	ret.right = rotation_matrix.to_vector3(0).to_float3();
	ret.up = rotation_matrix.to_vector3(1).to_float3();
	ret.front = rotation_matrix.to_vector3(2).to_float3();

	//	描画原点からの相対位置で視線行列を作る
	const math::matrixf view_matrix = make_view_matrix(parameters, ret.render_origin);
	const math::matrixf projection_matrix = parameters.make_projection_matrix();
	const math::matrixf view_projection_matrix = view_matrix * projection_matrix;
	ret.view_matrix = view_matrix.to_float4x4();
	ret.projection_matrix = projection_matrix.to_float4x4();
	ret.view_projection_matrix = view_projection_matrix.to_float4x4();

	ret.fov_y = parameters.fov_y;
	ret.clip_distance = parameters.get_effective_clip_distance();
	ret.screen_rect = parameters.screen_rect;

	ret.frustum_culling.create_planes(ret.view_projection_matrix);
	ret.frustum_culling.create_frustum_corners(view_projection_matrix.inverted().to_float4x4());
	return ret;
}

math::vector3d scene_view::compute_render_origin(const view_parameters& parameters, bool camera_relative)
{
	return camera_relative ? parameters.position : math::vector3d::zero();
}

void scene_view::fill_scene_constants(common_pipeline_states::scene_constants& constants, const scene_view_state* state) const
{
	constants.camera_position.x = relative_position.x;
	constants.camera_position.y = relative_position.y;
	constants.camera_position.z = relative_position.z;
	constants.camera_position.w = fov_y;

	constants.camera_right.x = right.x;
	constants.camera_right.y = right.y;
	constants.camera_right.z = right.z;
	constants.camera_right.w = tanf(fov_y * 0.5f);

	constants.camera_up.x = up.x;
	constants.camera_up.y = up.y;
	constants.camera_up.z = up.z;
	constants.camera_up.w = 0.0f;

	constants.camera_direction.x = front.x;
	constants.camera_direction.y = front.y;
	constants.camera_direction.z = front.z;
	constants.camera_direction.w = 0.0f;

	constants.viewport_rect = screen_rect;

	constants.camera_clip_distance.x = clip_distance.x;
	constants.camera_clip_distance.y = clip_distance.y;
	constants.camera_clip_distance.z = clip_distance.x * clip_distance.y;
	constants.camera_clip_distance.w = clip_distance.y - clip_distance.x;

	constants.z_buffer_parameteres.y = 1.0f;
	constants.z_buffer_parameteres.x = clip_distance.y / clip_distance.x - 1.0f;
	constants.z_buffer_parameteres.z = 1.0f / clip_distance.x - 1.0f / clip_distance.y;
	constants.z_buffer_parameteres.w = 1.0f / clip_distance.y;

	constants.view_transform = view_matrix;
	constants.projection_transform = projection_matrix;
	constants.view_projection_transform = view_projection_matrix;

	constants.inverse_view_transform = math::matrixf(view_matrix).inverted().to_float4x4();
	constants.inverse_projection_transform = math::matrixf(projection_matrix).inverted().to_float4x4();
	constants.inverse_view_projection_transform = math::matrixf(view_projection_matrix).inverted().to_float4x4();

	constants.previous_view_projection_transform = state ? state->get_previous_view_projection_matrix() : view_projection_matrix;
}
