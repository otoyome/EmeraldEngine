#include	"collision/cubic_sdf.h"

#include	"collision/mesh.h"

#include	<algorithm>
#include	<execution>
#include	<numeric>
#include	<cfloat>
#include	<cmath>
#include	<fstream>

#include	"cereal/cereal.hpp"
#include	"cereal/archives/binary.hpp"
#include	"cereal/types/vector.hpp"

bool collision::cubic_sdf::is_valid() const
{
	return resolution > 0 && !voxels.empty();
}

float collision::cubic_sdf::voxel_size() const
{
	//	ボクセルサイズ
	return resolution > 0 ? (bounds_max.x - bounds_min.x) / static_cast<float>(resolution) : 0.0f;
}

size_t collision::cubic_sdf::index_of(uint32_t x, uint32_t y, uint32_t z) const
{
	//	ボクセルインデックス(x,y,z)から線形インデックスを求める
	return (static_cast<size_t>(z) * resolution + y) * resolution + x;
}

math::float3 collision::cubic_sdf::voxel_center(uint32_t x, uint32_t y, uint32_t z) const
{
	//	ボクセル中心のワールド(ローカル)座標を求める
	const float size = voxel_size();
	return math::float3
	{
		bounds_min.x + (static_cast<float>(x) + 0.5f) * size,
		bounds_min.y + (static_cast<float>(y) + 0.5f) * size,
		bounds_min.z + (static_cast<float>(z) + 0.5f) * size,
	};
}

bool collision::cubic_sdf::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(resolution),
					CEREAL_NVP(bounds_min),
					CEREAL_NVP(bounds_max),
					CEREAL_NVP(voxels));
		}
		catch (...)
		{
			assert(!"cubic_sdf serialize failed.\n");
			return false;
		}

		return true;
	}
	return false;
}

bool collision::cubic_sdf::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(resolution),
					CEREAL_NVP(bounds_min),
					CEREAL_NVP(bounds_max),
					CEREAL_NVP(voxels));
		}
		catch (...)
		{
			assert(!"cubic_sdf deserialize failed.\n");
			return false;
		}

		return true;
	}
	return false;
}

bool collision::create_cubic_sdf(cubic_sdf& out_sdf, bounding_mesh& mesh, uint32_t resolution, float bounds_padding_rate)
{
	auto& positions = mesh.get_positions();
	auto& triangles = mesh.get_triangles();

	if (triangles.empty())
	{
		return false;
	}
	if (resolution == 0)
	{
		return false;
	}

	//	メッシュのAABBを求める
	math::float3 bounds_min = positions[0].to_float3();
	math::float3 bounds_max = positions[0].to_float3();
	for (auto& position : positions)
	{
		bounds_min.x = std::min(bounds_min.x, position.x);
		bounds_min.y = std::min(bounds_min.y, position.y);
		bounds_min.z = std::min(bounds_min.z, position.z);
		bounds_max.x = std::max(bounds_max.x, position.x);
		bounds_max.y = std::max(bounds_max.y, position.y);
		bounds_max.z = std::max(bounds_max.z, position.z);
	}

	//	バウンディングボックスの中心を求める
	const math::float3 center =
	{
		(bounds_min.x + bounds_max.x) * 0.5f,
		(bounds_min.y + bounds_max.y) * 0.5f,
		(bounds_min.z + bounds_max.z) * 0.5f,
	};

	const float extent_x = bounds_max.x - bounds_min.x;
	const float extent_y = bounds_max.y - bounds_min.y;
	const float extent_z = bounds_max.z - bounds_min.z;
	const float max_extent = std::max({ extent_x, extent_y, extent_z });
	const float half_extent = (max_extent * 0.5f) * (1.0f + bounds_padding_rate);

	out_sdf.resolution = resolution;
	out_sdf.bounds_min = { center.x - half_extent, center.y - half_extent, center.z - half_extent };
	out_sdf.bounds_max = { center.x + half_extent, center.y + half_extent, center.z + half_extent };
	out_sdf.voxels.assign(static_cast<size_t>(resolution) * resolution * resolution, 0.0f);

	//	ボクセル中心(ローカル)をワールドへ変換する行列
	math::float4x4 mesh_transform = mesh.get_transform_float4x4();

	std::vector<size_t> flat_indices(out_sdf.voxels.size());
	std::iota(flat_indices.begin(), flat_indices.end(), size_t{0});

	std::for_each(std::execution::par, flat_indices.begin(), flat_indices.end(),
		[&](size_t flat_index)
		{
			const uint32_t x = static_cast<uint32_t>(flat_index % resolution);
			const uint32_t y = static_cast<uint32_t>((flat_index / resolution) % resolution);
			const uint32_t z = static_cast<uint32_t>(flat_index / (static_cast<size_t>(resolution) * resolution));

			math::float3 voxel_center = out_sdf.voxel_center(x, y, z);
			voxel_center = math::vector3f::transform_coord(voxel_center, mesh_transform).to_float3();

			double world_distance = 0.0;
			mesh.distance_to_point(math::double3(voxel_center), world_distance);
			float distance = static_cast<float>(world_distance);

			//	内外判定
			if (mesh.is_point_inside(math::double3(voxel_center)))
				distance = -distance;

			out_sdf.voxels[flat_index] = distance;
		});

	return true;
}
