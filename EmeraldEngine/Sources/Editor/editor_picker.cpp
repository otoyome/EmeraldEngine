#include "Editor/editor_picker.h"

#if defined(USE_IMGUI)
#include <cmath>
#include <limits>

#include "Editor/editor_camera.h"
#include "Object/engine_context.h"
#include "Object/object_manager.h"
#include "Object/spatial_object.h"
#include "Object/Graphics/model_object.h"

namespace
{
	//	形の無いオブジェクトを当てる球の大きさ
	constexpr float sphere_radius_rate = 0.02f;
}

std::shared_ptr<game_object> editor_picker::pick(engine_context& context, const ImVec2& position, const ImVec2& image_size)
{
	std::shared_ptr<game_object> result;
	math::vector3d out_position;
	//	モデルだけ
	raycast(context, position, image_size, false, result, out_position);
	return result;
}

bool editor_picker::raycast(engine_context& context, const ImVec2& position, const ImVec2& image_size, bool include_shapeless,
							std::shared_ptr<game_object>& out_object, math::vector3d& out_position, float fallback_distance)
{
	out_object.reset();
	if (image_size.x <= 0.0f || image_size.y <= 0.0f)
		return false;

	//	ビューポートを映しているエディターカメラの視点
	object_manager& objects = context.get_objects();
	const std::shared_ptr<editor_camera_object> camera_object = objects.search_object<editor_camera_object>();
	if (!camera_object || !camera_object->get_camera())
		return false;
	const view_parameters& parameters = camera_object->get_camera()->get_parameters();

	//	クリック位置を通るレイ
	const float aspect = image_size.x / image_size.y;
	const float tan_half_fov = std::tan(parameters.fov_y * 0.5f);
	const float ndc_x = position.x / image_size.x * 2.0f - 1.0f;
	const float ndc_y = 1.0f - position.y / image_size.y * 2.0f;
	ray camera_ray;
	camera_ray.origin = math::vector3f(0.0f, 0.0f, 0.0f);
	camera_ray.direction = parameters.rotation.right() * (ndc_x * tan_half_fov * aspect) + parameters.rotation.up() * (ndc_y * tan_half_fov) + parameters.rotation.front();
	camera_ray.direction.normalize();

	float nearest = std::numeric_limits<float>::max();
	for (const std::shared_ptr<game_object>& object : objects.search_game_objects<spatial_object>())
	{
		if (!object || object->is_transient() || !object->is_active())
			continue;

		bool hit = false;
		if (object->check_type(model_object::type_name()))
		{
			hit = intersect_model(*static_cast<model_object*>(object.get()), camera_ray, parameters.position, nearest);
		}
		else if (include_shapeless)
		{
			const math::vector3d offset = static_cast<spatial_object*>(object.get())->get_world_translation() - parameters.position;
			const math::vector3f center(offset.to_float3());
			hit = intersect_sphere(center, center.length() * sphere_radius_rate, camera_ray, nearest);
		}
		if (hit)
			out_object = object;
	}

	const float distance = out_object ? nearest : fallback_distance;
	out_position = parameters.position + math::vector3d(camera_ray.direction.x * distance, camera_ray.direction.y * distance, camera_ray.direction.z * distance);
	return true;
}

bool editor_picker::intersect_model(model_object& model, const ray& camera_ray, const math::vector3d& camera_position, float& nearest)
{
	const model_instance_ptr instance = model.get_model_instance();
	if (!instance || !instance->get_resource())
		return false;
	const model_resource& resource = *instance->get_resource();

	const math::matrixf world_matrix = instance->get_world().to_relative_matrix(camera_position);

	bool hit = false;
	for (const model_resource::mesh& mesh : resource.get_meshes())
	{
		const auto position_it = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
		if (position_it == mesh.vertex_buffer_views.end())
			continue;
		const buffer_view& position_view = position_it->second;
		const buffer_view& index_view = mesh.index_buffer_view;
		if (position_view.stride_in_bytes < sizeof(math::float3) || position_view.row_data.empty() || index_view.stride_in_bytes == 0 || index_view.row_data.empty())
			continue;

		const math::matrixf inverse = (calculate_node_matrix(resource, mesh.node_index) * world_matrix).inverted();
		math::vector3f origin = camera_ray.origin;
		origin.transform_coord(inverse);
		math::vector3f direction = camera_ray.direction;
		direction.transform_normal(inverse);

		const size_t vertex_count = position_view.row_data.size() / position_view.stride_in_bytes;
		auto fetch_position = [&](size_t index)
		{
			const math::float3& p = *reinterpret_cast<const math::float3*>(position_view.row_data.data() + position_view.stride_in_bytes * index);
			return math::vector3f(p.x, p.y, p.z);
		};
		auto fetch_index = [&](size_t k) -> size_t
		{
			if (index_view.stride_in_bytes == 2)
				return reinterpret_cast<const USHORT*>(index_view.row_data.data())[k];
			if (index_view.stride_in_bytes == 1)
				return reinterpret_cast<const UCHAR*>(index_view.row_data.data())[k];
			return reinterpret_cast<const UINT*>(index_view.row_data.data())[k];
		};

		const size_t index_count = index_view.row_data.size() / index_view.stride_in_bytes;
		for (size_t k = 0; k + 2 < index_count; k += 3)
		{
			const size_t i0 = fetch_index(k), i1 = fetch_index(k + 1), i2 = fetch_index(k + 2);
			if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count)
				continue;

			float distance = 0.0f;
			if (intersect_triangle(origin, direction, fetch_position(i0), fetch_position(i1), fetch_position(i2), distance) && distance < nearest)
			{
				nearest = distance;
				hit = true;
			}
		}
	}
	return hit;
}

bool editor_picker::intersect_sphere(const math::vector3f& center, float radius, const ray& camera_ray, float& nearest)
{
	const math::vector3f to_center = center - camera_ray.origin;
	const float along = math::vector3f::dot(to_center, camera_ray.direction);
	if (along <= 0.0f)
		return false;
	const float distance_square = to_center.square_length() - along * along;
	const float radius_square = radius * radius;
	if (distance_square > radius_square)
		return false;

	const float distance = along - std::sqrt(radius_square - distance_square);
	if (distance >= nearest)
		return false;
	nearest = distance;
	return true;
}

bool editor_picker::intersect_triangle(const math::vector3f& origin, const math::vector3f& direction, const math::vector3f& a, const math::vector3f& b, const math::vector3f& c, float& out_distance)
{
	const math::vector3f edge1 = b - a;
	const math::vector3f edge2 = c - a;
	const math::vector3f p = math::vector3f::cross(direction, edge2);
	const float determinant = math::vector3f::dot(edge1, p);
	if (std::fabs(determinant) < 1.0e-12f)
		return false;

	const float inverse_determinant = 1.0f / determinant;
	const math::vector3f s = origin - a;
	const float u = math::vector3f::dot(s, p) * inverse_determinant;
	if (u < 0.0f || u > 1.0f)
		return false;

	const math::vector3f q = math::vector3f::cross(s, edge1);
	const float v = math::vector3f::dot(direction, q) * inverse_determinant;
	if (v < 0.0f || u + v > 1.0f)
		return false;

	out_distance = math::vector3f::dot(edge2, q) * inverse_determinant;
	return out_distance > 0.0f;
}

math::matrixf editor_picker::calculate_node_matrix(const model_resource& resource, size_t node_index)
{
	math::matrixf result;
	const auto& nodes = resource.get_nodes();
	while (node_index != model_resource::invalid_id && node_index < nodes.size())
	{
		const model_resource::node& node = nodes[node_index];
		result = result
			   * math::matrixf::scaling(node.scale.x, node.scale.y, node.scale.z)
			   * math::matrixf::rotation_quaternion(math::quaternionf(node.rotation))
			   * math::matrixf::translation(node.translation.x, node.translation.y, node.translation.z);
		node_index = node.parent_index;
	}
	return result;
}
#endif	//	defined(USE_IMGUI)
