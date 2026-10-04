#include "model_collision_builder.h"

#include <functional>

////////////////////////////////////////////////////////////////////////////////////////////////
bool collision::generate_bounding_mesh(bounding_mesh& out_mesh, model_resource_ptr resource, size_t index, generate_bounding_mesh_data data)
{
	auto& mesh = resource->get_meshes()[index];

	//	使用するインデックスバッファを選択
	const buffer_view* index_view = &mesh.index_buffer_view;
	if (data.lod_level >= 0)
	{
		if (data.lod_level >= static_cast<int>(mesh.lod_meshs.size()))
			return false;
		index_view = &mesh.lod_meshs[data.lod_level].index_buffer_view;
	}
	if (index_view->count() == 0)
		return false;

	auto& nodes = resource->get_nodes();
	std::function<math::matrixf(size_t index)> calculate_transform_matrix = [&](size_t node_index)
	{
		if (node_index == model_resource::invalid_id)
			return math::matrixf();

		auto& node = nodes[node_index];
		math::matrixf transform;
		transform = math::matrixf::scaling(node.scale.x, node.scale.y, node.scale.z)
				  * math::matrixf::rotation_quaternion(math::quaternionf(node.rotation))
				  * math::matrixf::translation(node.translation.x, node.translation.y, node.translation.z);
		if (node.parent_index != model_resource::invalid_id)
			transform *= calculate_transform_matrix(node.parent_index);

		return transform;
	};

	math::matrixf local_transform_matrix = calculate_transform_matrix(mesh.node_index);

	//	ワールド行列を分解しスケールは頂点へ焼き込む
	math::vector3f WorldScale, WorldTranslation;
	math::quaternionf WorldRotation;
	data.WorldMatrix.decompose(WorldScale, WorldRotation, WorldTranslation);
	local_transform_matrix = local_transform_matrix * math::matrixf::scaling(WorldScale);

	auto it = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
	if (it == mesh.vertex_buffer_views.end())
		return false;

	std::vector<math::float3> positions;
	std::vector<bounding_mesh::triangle> triangles;
	{
		auto p = reinterpret_cast<const math::float3*>(it->second.row_data.data());
		for (UINT index = 0; index < it->second.count(); ++index)
		{
			math::vector3f P(*(p + index));
			positions.emplace_back() = (math::vector3f::transform_coord(P, local_transform_matrix)).to_float3();
		}

		auto fetch_index = [&](UINT k) -> UINT
		{
			if (index_view->stride_in_bytes == 2)
				return reinterpret_cast<const USHORT*>(index_view->row_data.data())[k];
			if (index_view->stride_in_bytes == 1)
				return reinterpret_cast<const UCHAR*>(index_view->row_data.data())[k];
			return reinterpret_cast<const UINT*>(index_view->row_data.data())[k];
		};
		UINT index1 = data.reverse_flag ? 2 : 0;
		UINT index2 = 1;
		UINT index3 = data.reverse_flag ? 0 : 2;
		for (UINT index = 0; index < index_view->count(); index += 3)
		{
			bounding_mesh::triangle& triangle = triangles.emplace_back();
			triangle.a = fetch_index(index + index1);
			triangle.b = fetch_index(index + index2);
			triangle.c = fetch_index(index + index3);

			//	事前計算
			auto tp = positions.data();
			auto A = math::vector3f(*(tp + triangle.a));
			auto B = math::vector3f(*(tp + triangle.b));
			auto C = math::vector3f(*(tp + triangle.c));
			auto AB = B - A;
			auto BC = C - B;
			auto CA = A - C;
			triangle.ab = AB.to_float3();
			triangle.bc = BC.to_float3();
			triangle.ca = CA.to_float3();

			auto Normal = math::vector3f::cross(AB, BC);
			triangle.normal = Normal.normalized().to_float3();

			auto Min = (math::min)(A, (math::min)(B, C));
			auto Max = (math::max)(A, (math::max)(B, C));
			triangle.min = Min.to_float3();
			triangle.max = Max.to_float3();
			triangle.lab = collision::line(collision::line::types::segment, *(tp + triangle.a), *(tp + triangle.b));
			triangle.lbc = collision::line(collision::line::types::segment, *(tp + triangle.b), *(tp + triangle.c));
			triangle.lca = collision::line(collision::line::types::segment, *(tp + triangle.c), *(tp + triangle.a));
		}
	}

	out_mesh = collision::bounding_mesh(positions, triangles, data.separate_level);

	{
		math::float4 rotation;
		math::float3 translation;
		rotation = WorldRotation.to_float4();
		translation = WorldTranslation.to_float3();

		out_mesh.set_rotation(rotation);
		out_mesh.set_position(translation);
	}

	return true;
}

bool collision::create_cubic_sdf(cubic_sdf& out_sdf, model_resource_ptr resource, size_t index, const generate_bounding_mesh_data& mesh_data, uint32_t resolution, float bounds_padding_rate)
{
	if (!resource)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "create_cubic_sdf failed : resource is null");
		return false;
	}

	bounding_mesh mesh;
	if (!generate_bounding_mesh(mesh, resource, index, mesh_data))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "create_cubic_sdf failed : generate_bounding_mesh failed");
		return false;
	}

	if (!create_cubic_sdf(out_sdf, mesh, resolution, bounds_padding_rate))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "create_cubic_sdf failed : mesh empty or resolution is 0");
		return false;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::model, "create_cubic_sdf successed[resolution=%u, triangle_count=%zu]",
		resolution, mesh.get_triangles().size());
	return true;
}
