#include "model_collision_builder.h"

#include <fstream>
#include <functional>

namespace
{
	//	メッシュ1つ分の頂点と三角形を配列の末尾へ追加
	bool append_mesh_geometry(std::vector<math::vector3f>& positions, std::vector<collision::bounding_mesh::triangle>* triangles,
							  model_resource_ptr resource, size_t index, const collision::generate_bounding_mesh_data& data)
	{
		using collision::bounding_mesh;

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

		const std::uint32_t base_index = static_cast<std::uint32_t>(positions.size());
		{
			auto p = reinterpret_cast<const math::float3*>(it->second.row_data.data());
			for (UINT index = 0; index < it->second.count(); ++index)
			{
				math::vector3f P(*(p + index));
				positions.emplace_back() = math::vector3f::transform_coord(P, local_transform_matrix);
			}

			if (!triangles)
				return true;

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
				bounding_mesh::triangle& triangle = triangles->emplace_back();
				triangle.a = base_index + fetch_index(index + index1);
				triangle.b = base_index + fetch_index(index + index2);
				triangle.c = base_index + fetch_index(index + index3);

				//	事前計算
				auto tp = positions.data();
				const auto& A = *(tp + triangle.a);
				const auto& B = *(tp + triangle.b);
				const auto& C = *(tp + triangle.c);
				auto AB = B - A;
				auto BC = C - B;
				auto CA = A - C;
				triangle.ab = AB;
				triangle.bc = BC;
				triangle.ca = CA;

				auto Normal = math::vector3f::cross(AB, BC);
				triangle.normal = Normal.normalized();

				auto Min = (math::min)(A, (math::min)(B, C));
				auto Max = (math::max)(A, (math::max)(B, C));
				triangle.min = Min;
				triangle.max = Max;
				triangle.lab = collision::linef(collision::linef::types::segment, A.to_float3(), B.to_float3());
				triangle.lbc = collision::linef(collision::linef::types::segment, B.to_float3(), C.to_float3());
				triangle.lca = collision::linef(collision::linef::types::segment, C.to_float3(), A.to_float3());
			}
		}
		return true;
	}

	//	ワールド行列の回転と平行移動を当たり判定メッシュへ設定する
	void apply_world_transform(collision::bounding_mesh& out_mesh, const collision::generate_bounding_mesh_data& data)
	{
		math::vector3f WorldScale, WorldTranslation;
		math::quaternionf WorldRotation;
		data.WorldMatrix.decompose(WorldScale, WorldRotation, WorldTranslation);

		math::double4 rotation;
		math::double3 translation;
		rotation = WorldRotation.to_double4();
		translation = WorldTranslation.to_double3();

		out_mesh.set_rotation(rotation);
		out_mesh.set_position(translation);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
bool collision::generate_bounding_mesh(bounding_mesh& out_mesh, model_resource_ptr resource, size_t index, generate_bounding_mesh_data data)
{
	std::vector<math::vector3f> positions;
	std::vector<bounding_mesh::triangle> triangles;
	if (!append_mesh_geometry(positions, &triangles, resource, index, data))
		return false;

	out_mesh = collision::bounding_mesh(positions, triangles);
	apply_world_transform(out_mesh, data);
	return true;
}

bool collision::generate_combined_bounding_mesh(bounding_mesh& out_mesh, model_resource_ptr resource, const std::vector<size_t>& indices, generate_bounding_mesh_data data)
{
	std::vector<math::vector3f> positions;
	std::vector<bounding_mesh::triangle> triangles;
	for (size_t index : indices)
	{
		//	生成できないメッシュは飛ばす
		append_mesh_geometry(positions, &triangles, resource, index, data);
	}
	if (triangles.empty())
		return false;

	out_mesh = collision::bounding_mesh(positions, triangles);
	apply_world_transform(out_mesh, data);
	return true;
}

bool collision::calculate_bounding_mesh_bounds(model_resource_ptr resource, size_t index, const generate_bounding_mesh_data& data, math::float3& out_min, math::float3& out_max)
{
	std::vector<math::vector3f> positions;
	if (!append_mesh_geometry(positions, nullptr, resource, index, data) || positions.empty())
		return false;

	math::vector3f Min = positions[0];
	math::vector3f Max = positions[0];
	for (auto& position : positions)
	{
		Min = (math::min)(Min, position);
		Max = (math::max)(Max, position);
	}
	out_min = Min.to_float3();
	out_max = Max.to_float3();
	return true;
}

std::filesystem::path collision::combined_bounding_mesh_info::make_mesh_path(std::string_view model_path, size_t combined_index)
{
	std::filesystem::path path{ std::string(model_path) };
	path.replace_extension("combined" + std::to_string(combined_index) + "." + std::string(bounding_mesh::extension));
	return path;
}

std::filesystem::path collision::combined_bounding_mesh_info::make_info_path(std::string_view model_path)
{
	std::filesystem::path path{ std::string(model_path) };
	path.replace_extension(std::string(extension));
	return path;
}

bool collision::combined_bounding_mesh_info::serialize(std::string_view file_path) const
{
	std::ofstream ostream(std::string(file_path), std::ios::binary);
	if (!ostream.is_open())
		return false;

	const std::uint32_t count = static_cast<std::uint32_t>(material_indices.size());
	ostream.write(reinterpret_cast<const char*>(&count), sizeof(count));
	ostream.write(reinterpret_cast<const char*>(material_indices.data()), sizeof(std::uint32_t) * count);
	return ostream.good();
}

bool collision::combined_bounding_mesh_info::deserialize(std::string_view file_path)
{
	std::ifstream istream(std::string(file_path), std::ios::binary);
	if (!istream.is_open())
		return false;

	std::uint32_t count = 0;
	istream.read(reinterpret_cast<char*>(&count), sizeof(count));
	if (!istream.good())
		return false;

	material_indices.resize(count);
	if (count > 0)
		istream.read(reinterpret_cast<char*>(material_indices.data()), sizeof(std::uint32_t) * count);
	return !istream.fail();
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
