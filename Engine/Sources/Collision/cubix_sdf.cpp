#include	"collision/cubix_sdf.h"

#include	"collision/mesh.h"

#include	<algorithm>
#include	<execution>
#include	<numeric>
#include	<cfloat>
#include	<cmath>

namespace
{
	using namespace DirectX;

	//	点と三角形の最近接点までの距離(二乗)を求める
	float XM_CALLCONV squared_distance_point_triangle(DirectX::XMVECTOR P, DirectX::XMVECTOR A, DirectX::XMVECTOR B, DirectX::XMVECTOR C)
	{
		XMVECTOR AB = XMVectorSubtract(B, A);
		XMVECTOR AC = XMVectorSubtract(C, A);
		XMVECTOR BC = XMVectorSubtract(C, B);
		XMVECTOR AP = XMVectorSubtract(P, A);

		float d1 = XMVectorGetX(XMVector3Dot(AB, AP));
		float d2 = XMVectorGetX(XMVector3Dot(AC, AP));
		if (d1 <= 0.0f && d2 <= 0.0f)
			return XMVectorGetX(XMVector3LengthSq(AP));	//	最近接点は頂点A

		XMVECTOR BP = XMVectorSubtract(P, B);
		float d3 = XMVectorGetX(XMVector3Dot(AB, BP));
		float d4 = XMVectorGetX(XMVector3Dot(AC, BP));
		if (d3 >= 0.0f && d4 <= d3)
			return XMVectorGetX(XMVector3LengthSq(BP));	//	最近接点は頂点B

		XMVECTOR CP = XMVectorSubtract(P, C);
		float d5 = XMVectorGetX(XMVector3Dot(AB, CP));
		float d6 = XMVectorGetX(XMVector3Dot(AC, CP));
		if (d6 >= 0.0f && d5 <= d6)
			return XMVectorGetX(XMVector3LengthSq(CP));	//	最近接点は頂点C

		float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
		{
			XMVECTOR closest = XMVectorAdd(A, XMVectorScale(AB, d1 / (d1 - d3)));
			return XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(P, closest)));	//	最近接点は辺AB上
		}

		float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
		{
			XMVECTOR closest = XMVectorAdd(A, XMVectorScale(AC, d2 / (d2 - d6)));
			return XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(P, closest)));	//	最近接点は辺AC上
		}

		float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		{
			float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			XMVECTOR closest = XMVectorAdd(B, XMVectorScale(BC, w));
			return XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(P, closest)));	//	最近接点は辺BC上
		}

		//	最近接点は三角形の内部
		float denom = 1.0f / (va + vb + vc);
		XMVECTOR closest = XMVectorAdd(A, XMVectorAdd(XMVectorScale(AB, vb * denom), XMVectorScale(AC, vc * denom)));
		return XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(P, closest)));
	}

	//	レイと三角形の交差判定(Moller-Trumbore)
	bool XM_CALLCONV ray_intersects_triangle(DirectX::XMVECTOR origin, DirectX::XMVECTOR direction, DirectX::XMVECTOR A, DirectX::XMVECTOR B, DirectX::XMVECTOR C)
	{
		constexpr float epsilon = 1e-6f;

		XMVECTOR AB = XMVectorSubtract(B, A);
		XMVECTOR AC = XMVectorSubtract(C, A);
		XMVECTOR pvec = XMVector3Cross(direction, AC);
		float det = XMVectorGetX(XMVector3Dot(AB, pvec));
		if (fabsf(det) < epsilon)
			return false;	//	レイと三角形が平行

		float inv_det = 1.0f / det;
		XMVECTOR tvec = XMVectorSubtract(origin, A);
		float u = XMVectorGetX(XMVector3Dot(tvec, pvec)) * inv_det;
		if (u < 0.0f || u > 1.0f)
			return false;

		XMVECTOR qvec = XMVector3Cross(tvec, AB);
		float v = XMVectorGetX(XMVector3Dot(direction, qvec)) * inv_det;
		if (v < 0.0f || u + v > 1.0f)
			return false;

		float t = XMVectorGetX(XMVector3Dot(AC, qvec)) * inv_det;
		return t > epsilon;	//	原点より前方でのみ交差とみなす
	}

	//	内外判定に使うレイの方向
	//	@remainder	軸に平行な方向だと辺・頂点をかすめて交差数がぶれやすいため、斜め方向を使う
	constexpr DirectX::XMFLOAT3 inside_test_ray_directions[] =
	{
		{ 0.5773503f,  0.5773503f,  0.5773503f},
		{ 0.7071068f, -0.7071068f,  0.4082483f},
		{-0.4082483f,  0.5773503f, -0.7071068f},
	};

	//	複数方向へのレイと全三角形との交差数(偶奇)の多数決で、点がメッシュの内側かどうかを判定する
	bool is_point_inside(const DirectX::XMFLOAT3& point,
						  const std::vector<DirectX::XMFLOAT3>& positions,
						  const std::vector<collision::bounding_mesh::triangle>& triangles)
	{
		XMVECTOR P = XMLoadFloat3(&point);

		constexpr size_t direction_count = std::size(inside_test_ray_directions);
		XMVECTOR directions[direction_count];
		UINT hit_counts[direction_count] = {};
		for (size_t index = 0; index < direction_count; ++index)
			directions[index] = XMLoadFloat3(&inside_test_ray_directions[index]);

		for (auto& triangle : triangles)
		{
			XMVECTOR A = XMLoadFloat3(&positions[triangle.a]);
			XMVECTOR B = XMLoadFloat3(&positions[triangle.b]);
			XMVECTOR C = XMLoadFloat3(&positions[triangle.c]);

			for (size_t index = 0; index < direction_count; ++index)
			{
				if (ray_intersects_triangle(P, directions[index], A, B, C))
					++hit_counts[index];
			}
		}

		UINT inside_votes = 0;
		for (auto hit_count : hit_counts)
		{
			if ((hit_count % 2) == 1)
				++inside_votes;
		}
		return inside_votes * 2 > direction_count;
	}
}

bool collision::create_cubic_sdf(cubix_sdf& out_sdf, bounding_mesh& mesh, uint32_t resolution, float bounds_padding_rate)
{
	auto& positions = mesh.get_positions();
	auto& triangles = mesh.get_triangles();

	if (triangles.empty())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "create_cubic_sdf failed : mesh empty");
		return false;
	}
	if (resolution == 0)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "create_cubic_sdf failed : resolution must be greater than 0");
		return false;
	}

	//	メッシュのAABBを求める
	DirectX::XMFLOAT3 bounds_min = positions[0];
	DirectX::XMFLOAT3 bounds_max = positions[0];
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
	const DirectX::XMFLOAT3 center =
	{
		(bounds_min.x + bounds_max.x) * 0.5f,
		(bounds_min.y + bounds_max.y) * 0.5f,
		(bounds_min.z + bounds_max.z) * 0.5f,
	};

	//	立方体状のSDFにするため、最大の辺長を基準に各軸へ余白(padding)を加えて正立方体の解像度を作る
	const float extent_x = bounds_max.x - bounds_min.x;
	const float extent_y = bounds_max.y - bounds_min.y;
	const float extent_z = bounds_max.z - bounds_min.z;
	const float max_extent = std::max({ extent_x, extent_y, extent_z });
	const float half_extent = (max_extent * 0.5f) * (1.0f + bounds_padding_rate);

	out_sdf.resolution = resolution;
	out_sdf.bounds_min = { center.x - half_extent, center.y - half_extent, center.z - half_extent };
	out_sdf.bounds_max = { center.x + half_extent, center.y + half_extent, center.z + half_extent };
	out_sdf.voxels.assign(static_cast<size_t>(resolution) * resolution * resolution, 0.0f);

	//	フラットインデックスの配列を用意し、ボクセル単位で並列に距離計算を行う
	//	(各ボクセルの計算は他のボクセルに依存しないため完全に並列化可能)
	std::vector<size_t> flat_indices(out_sdf.voxels.size());
	std::iota(flat_indices.begin(), flat_indices.end(), size_t{0});

	std::for_each(std::execution::par, flat_indices.begin(), flat_indices.end(),
		[&](size_t flat_index)
		{
			const uint32_t x = static_cast<uint32_t>(flat_index % resolution);
			const uint32_t y = static_cast<uint32_t>((flat_index / resolution) % resolution);
			const uint32_t z = static_cast<uint32_t>(flat_index / (static_cast<size_t>(resolution) * resolution));

			const DirectX::XMFLOAT3 voxel_center = out_sdf.voxel_center(x, y, z);
			DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&voxel_center);

			//	全三角形との距離を総当りで調べ、最短のものを採用する
			//	@remainder	三角形数が多い場合はBVH/Octreeを併用した高速化を推奨
			float min_squared_distance = FLT_MAX;
			for (auto& triangle : triangles)
			{
				DirectX::XMVECTOR A = DirectX::XMLoadFloat3(&positions[triangle.a]);
				DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&positions[triangle.b]);
				DirectX::XMVECTOR C = DirectX::XMLoadFloat3(&positions[triangle.c]);
				float squared_distance = squared_distance_point_triangle(P, A, B, C);
				min_squared_distance = std::min(min_squared_distance, squared_distance);
			}
			float distance = std::sqrt(min_squared_distance);

			//	レイキャストの交差パリティで内外判定し、内側なら符号を反転する
			if (is_point_inside(voxel_center, positions, triangles))
				distance = -distance;

			out_sdf.voxels[flat_index] = distance;
		});

	DEBUG_LOG_CATEGORY(logger::categorys::model, "create_cubic_sdf successed[resolution=%u, triangle_count=%zu]",
		resolution, triangles.size());
	return true;
}
