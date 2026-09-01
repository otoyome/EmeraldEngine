#include "collision/mesh.h"

#include "collision/sphere.h"
#include "collision/capsule.h"
#include "collision/box.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"

#include <DirectXCollision.h>
#include <thread>

namespace
{
	inline bool plane_aabb(const DirectX::XMFLOAT3& n, const float d, const DirectX::XMFLOAT3& c, const DirectX::XMFLOAT3& h)
	{
		float e = h.x * fabsf(n.x)
				+ h.y * fabsf(n.y)
				+ h.z * fabsf(n.z);
		float s = (c.x * n.x + c.y * n.y + c.z * n.z) + d;
		if (s - e > 0) return false; // 半空間外(法線の逆側)
		if (s + e < 0) return false; // 半空間内(法線側)
		return true;
	}

	inline bool triangle_aabb(const DirectX::XMFLOAT3& u0,
							  const DirectX::XMFLOAT3& u1,
							  const DirectX::XMFLOAT3& u2,
							  const DirectX::XMFLOAT3& n,
							  const collision::inner::axis_aligned_bounding_box& box)
	{
		// 三角形の頂点をAABB中心を原点とした座標系に移動
		DirectX::XMFLOAT3 v[3];
		v[0].x = u0.x - box.get_position().x;
		v[0].y = u0.y - box.get_position().y;
		v[0].z = u0.z - box.get_position().z;
		v[1].x = u1.x - box.get_position().x;
		v[1].y = u1.y - box.get_position().y;
		v[1].z = u1.z - box.get_position().z;
		v[2].x = u2.x - box.get_position().x;
		v[2].y = u2.y - box.get_position().y;
		v[2].z = u2.z - box.get_position().z;

		// 1. 三角形のAABBとAABBの交差判定
		DirectX::XMFLOAT3 bmin, bmax;
		bmin.x = -box.get_local_scale().x;
		bmin.y = -box.get_local_scale().y;
		bmin.z = -box.get_local_scale().z;
		bmax = box.get_local_scale();
		
		DirectX::XMFLOAT3 tmin = v[0], tmax = v[0];
		for(int j = 1; j < 3; ++j)
		{
			if(v[j].x < tmin.x) tmin.x = v[j].x;
			if(v[j].x > tmax.x) tmax.x = v[j].x;
			if(v[j].y < tmin.y) tmin.y = v[j].y;
			if(v[j].y > tmax.y) tmax.y = v[j].y;
			if(v[j].z < tmin.z) tmin.z = v[j].z;
			if(v[j].z > tmax.z) tmax.z = v[j].z;
		}

		// 各軸での重なり判定
		int overlap = 0;
		if(!(tmax.x < bmin.x || tmin.x > bmax.x)) overlap++;
		if(!(tmax.y < bmin.y || tmin.y > bmax.y)) overlap++;
		if(!(tmax.z < bmin.z || tmin.z > bmax.z)) overlap++;

		//	3軸すべてで重なっていたらAABB同士が重なっている
		if(overlap < 3)
			return false;

		// 2. AABBと三角形を含む平面との交差判定
		if(!plane_aabb(n, -(n.x * v[0].x + n.y * v[0].y + n.z * v[0].z), {0.0f, 0.0f, 0.0f}, box.get_local_scale()))
			return false;

		// 3. AABBの各軸方向とエッジのテスト
		DirectX::XMFLOAT3 e[3] = { {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} };
		DirectX::XMFLOAT3 f[3] = {};
		f[0].x = v[1].x - v[0].x;
		f[0].y = v[1].y - v[0].y;
		f[0].z = v[1].z - v[0].z;
		f[1].x = v[2].x - v[1].x;
		f[1].y = v[2].y - v[1].y;
		f[1].z = v[2].z - v[1].z;
		f[2].x = v[0].x - v[2].x;
		f[2].y = v[0].y - v[2].y;
		f[2].z = v[0].z - v[2].z;
		for(int i = 0; i < 3; ++i)
		{
			for(int j = 0; j < 3; ++j)
			{
				DirectX::XMFLOAT3 a, p;
				DirectX::XMVECTOR A = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&e[i]), DirectX::XMLoadFloat3(&f[j]));
				DirectX::XMStoreFloat3(&a, A);
				p.x = DirectX::XMVectorGetX(DirectX::XMVector3Dot(A, DirectX::XMLoadFloat3(&v[0])));
				p.y = DirectX::XMVectorGetX(DirectX::XMVector3Dot(A, DirectX::XMLoadFloat3(&v[1])));
				p.z = DirectX::XMVectorGetX(DirectX::XMVector3Dot(A, DirectX::XMLoadFloat3(&v[2])));
				float r = box.get_local_scale().x * fabs(a.x)
						+ box.get_local_scale().y * fabs(a.y)
						+ box.get_local_scale().z * fabs(a.z);
				if(std::min(p.x, std::min(p.y, p.z)) > r || std::max(p.x, std::max(p.y, p.z)) < -r)
					return false;
			}
		}

		return true;
	}

	inline UINT calculate_array_index_from_point(const DirectX::XMFLOAT3& point, const DirectX::XMFLOAT3& area_min, const DirectX::XMFLOAT3& area_unit, DirectX::XMUINT3 separate_count)
	{
		DirectX::XMFLOAT3 p;
		p.x = (point.x - area_min.x) / area_unit.x;
		p.y = (point.y - area_min.y) / area_unit.y;
		p.z = (point.z - area_min.z) / area_unit.z;
		return static_cast<UINT>(p.x) + static_cast<UINT>(separate_count.x * p.y) + static_cast<UINT>(separate_count.x * separate_count.y * p.z);
	}

	std::vector<UINT> calculate_check_order_list(const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max, const DirectX::XMFLOAT3& area_min, const DirectX::XMFLOAT3& area_unit, DirectX::XMUINT3 separate_count)
	{
		DirectX::XMFLOAT3 vector;
		vector.x = max.x - min.x;
		vector.y = max.y - min.y;
		vector.z = max.z - min.z;

		DirectX::XMUINT3 axis_count;
		axis_count.x = static_cast<UINT>(ceilf((vector.x / area_unit.x + 0.001f)));
		axis_count.y = static_cast<UINT>(ceilf((vector.y / area_unit.y + 0.001f)));
		axis_count.z = static_cast<UINT>(ceilf((vector.z / area_unit.z + 0.001f)));

		UINT square_separate_count = separate_count.x * separate_count.y;
		UINT cube_separate_count = separate_count.x * separate_count.y * separate_count.z;
		std::vector<UINT> order_list;
		order_list.reserve(axis_count.x * axis_count.y * axis_count.z);

		const UINT base_index_x = static_cast<UINT>((min.x - area_min.x) / area_unit.x);
		const UINT base_index_y = static_cast<UINT>((min.y - area_min.y) / area_unit.y);
		const UINT base_index_z = static_cast<UINT>((min.z - area_min.z) / area_unit.z);
		for (UINT x = 0; x < axis_count.x; ++x)
		{
			const UINT index_x = base_index_x + x;
			for (UINT y = 0; y < axis_count.y; ++y)
			{
				const UINT index_y = base_index_y + y;
				const UINT index_xy = index_x + index_y * separate_count.x;
				for (UINT z = 0; z < axis_count.z; ++z)
				{
					const UINT index_z = base_index_z + z;
					const UINT index = index_xy + index_z * square_separate_count;
					if (index < cube_separate_count)
					{
						order_list.emplace_back(index);
					}
				}
			}
		}
		return order_list;
	}


	bool XM_CALLCONV calculate_nearlest_point(DirectX::XMVECTOR P,
											  DirectX::XMVECTOR A, DirectX::XMVECTOR B, DirectX::XMVECTOR C,
											  DirectX::XMVECTOR EdgeAB, DirectX::XMVECTOR EdgeBC, DirectX::XMVECTOR EdgeAC,
											  DirectX::XMVECTOR& NearlestPoint)
	{
		DirectX::XMVECTOR AP = DirectX::XMVectorSubtract(P, A);

		float d1 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(EdgeAB, AP));
		float d2 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(EdgeAC, AP));
		if (d1 <= 0.0f && d2 <= 0.0f)
		{
			NearlestPoint = A;
			return false;
		}

		DirectX::XMVECTOR bp = DirectX::XMVectorSubtract(P, B);
		float d3 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(EdgeAB, bp));
		float d4 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(EdgeAC, bp));
		if (d3 >= 0.0f && d4 <= d3)
		{
			NearlestPoint = B;
			return false;
		}

		DirectX::XMVECTOR cp = DirectX::XMVectorSubtract(P, C);
		float d5 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(EdgeAB, cp));
		float d6 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(EdgeAC, cp));
		if (d6 >= 0.0f && d5 <= d6)
		{
			NearlestPoint = C;
			return false;
		}

		float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
		{
			NearlestPoint = DirectX::XMVectorAdd(DirectX::XMVectorScale(EdgeAB, d1 / (d1 - d3)), A);
			return false;
		}

		float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
		{
			NearlestPoint = DirectX::XMVectorAdd(DirectX::XMVectorScale(EdgeAC, d2 / (d2 - d6)), A);
			return false;
		}

		float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		{
			float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			NearlestPoint = DirectX::XMVectorAdd(DirectX::XMVectorScale(EdgeBC, w), B);
			return false;
		}

		//	最近点が三角形内の点と判断
		float denom = 1.0f / (va + vb + vc);
		DirectX::XMVECTOR ab = DirectX::XMVectorScale(EdgeAB, vb * denom);
		DirectX::XMVECTOR ac = DirectX::XMVectorScale(EdgeAC, vc * denom);
		NearlestPoint = DirectX::XMVectorAdd(DirectX::XMVectorAdd(ab, ac), A);
		return true;
	}

	bool XM_CALLCONV intersect_ray(DirectX::XMVECTOR Start, DirectX::XMVECTOR Direction,
								   DirectX::XMVECTOR A, DirectX::XMVECTOR B, DirectX::XMVECTOR C,
								   DirectX::XMVECTOR EdgeAB, DirectX::XMVECTOR EdgeBC, DirectX::XMVECTOR EdgeCA,
								   DirectX::XMVECTOR Normal,
								   DirectX::XMVECTOR& InsertPoint, float& distance, float max_delta = 1.0f)
	{
		float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Direction, Normal));
		if (dot >= 0.0f)
			return	false;

		float dot2 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSubtract(A, Start), Normal));
		float delta = (dot2 == 0.0f) ? 0 : (dot2 / dot);
		if (delta < 0.0f || delta > max_delta)
			return	false;

		DirectX::XMVECTOR IP = DirectX::XMVectorAdd(DirectX::XMVectorScale(Direction, delta), Start);

		DirectX::XMVECTOR Work;
		Work = DirectX::XMVector3Cross(EdgeAB, DirectX::XMVectorSubtract(A, IP));
		if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(Work, Normal)) < 0.0f)
			return false;

		Work = DirectX::XMVector3Cross(EdgeBC, DirectX::XMVectorSubtract(B, IP));
		if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(Work, Normal)) < 0.0f)
			return false;

		Work = DirectX::XMVector3Cross(EdgeCA, DirectX::XMVectorSubtract(C, IP));
		if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(Work, Normal)) < 0.0f)
			return false;

		InsertPoint = IP;
		distance = delta;
		return	true;
	}

	bool XM_CALLCONV intersect_ray_moller(DirectX::XMVECTOR Start, DirectX::XMVECTOR Direction,
										  DirectX::XMVECTOR A, DirectX::XMVECTOR B, DirectX::XMVECTOR C,
										  DirectX::XMVECTOR EdgeAB, DirectX::XMVECTOR EdgeCA,
										  DirectX::XMVECTOR& InsertPoint, float& distance, float max_delta = 1.0f)
	{
		DirectX::XMVECTOR e1 = EdgeAB;
		DirectX::XMVECTOR e2 = DirectX::XMVectorNegate(EdgeCA);

		DirectX::XMVECTOR alpha = DirectX::XMVector3Cross(Direction, e2);
		float det = DirectX::XMVectorGetX(DirectX::XMVector3Dot(e1, alpha));
		if (det <= 1e-6f)
			return false;

		float inv_det = 1.0f / det;
		DirectX::XMVECTOR r = DirectX::XMVectorSubtract(Start, A);

		float u = DirectX::XMVectorGetX(DirectX::XMVector3Dot(alpha, r)) * inv_det;
		if (u < 0.0f || u > 1.0f)
			return false;

		DirectX::XMVECTOR beta = DirectX::XMVector3Cross(r, e1);

		float v = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Direction, beta)) * inv_det;
		if (v < 0.0f || u + v > 1.0f)
			return false;

		float t = DirectX::XMVectorGetX(DirectX::XMVector3Dot(e2, beta)) * inv_det;
		if (t < 0.0f || t > max_delta)
			return false;

		InsertPoint = DirectX::XMVectorAdd(DirectX::XMVectorScale(Direction, t), Start);
		distance = t;
		return	true;
	}
}

using namespace collision;

// CEREALバージョン定義
CEREAL_CLASS_VERSION(bounding_mesh::triangle, 1)
CEREAL_CLASS_VERSION(bounding_mesh::block_data, 1)

template<class Archive>
void bounding_mesh::triangle::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(a), CEREAL_NVP(b), CEREAL_NVP(c));
}

template<class Archive>
void bounding_mesh::block_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(center), CEREAL_NVP(triangles));
}

const DirectX::XMFLOAT3& bounding_mesh::get_local_scale() const
{
	static const DirectX::XMFLOAT3 scale = {1.0f, 1.0f, 1.0f};
	return	scale;
}

void bounding_mesh::set_local_scale(const DirectX::XMFLOAT3& scale)
{
	//	何もしない
}

bounding_mesh::bounding_mesh()
: bounding_object({ 1, 1, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0 })
{
}

bounding_mesh::bounding_mesh(std::vector<DirectX::XMFLOAT3>& positions, std::vector<triangle>& triangles, UINT separate_level)
	: bounding_object({1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0})
	, positions(positions)
	, triangles(triangles)
	, separate_count(1, 1, 1)
{
	//	幅算出
	//include_corner(area_min, area_max);
	area_min = area_max = positions[0];
	for (auto& position : positions)
	{
		area_min.x	= std::min(position.x, area_min.x);
		area_min.y	= std::min(position.y, area_min.y);
		area_min.z	= std::min(position.z, area_min.z);
		area_max.x	= std::max(position.x, area_max.x);
		area_max.y	= std::max(position.y, area_max.y);
		area_max.z	= std::max(position.z, area_max.z);
	}

	area_min.x -= 1.0f; area_min.y -= 1.0f; area_min.z -= 1.0f;
	area_max.x += 1.0f; area_max.y += 1.0f; area_max.z += 1.0f;

	area_size.x = area_max.x - area_min.x;
	area_size.y = area_max.y - area_min.y;
	area_size.z = area_max.z - area_min.z;

	if (triangles.size() <= 1024)
		separate_level = 0;

	{
		UINT temp_separate_count = 1u << separate_level;
		separate_count.x = temp_separate_count;
		separate_count.y = temp_separate_count;
		separate_count.z = temp_separate_count;
		for (; separate_count.x >= 1; --separate_count.x)
		{
			float temp = area_size.x / static_cast<float>(separate_count.x);
			if (temp >= separate_level) break;
		}
		for (; separate_count.y >= 1; --separate_count.y)
		{
			float temp = area_size.y / static_cast<float>(separate_count.y);
			if (temp >= separate_level) break;
		}
		for (; separate_count.z >= 1; --separate_count.z)
		{
			float temp = area_size.z / static_cast<float>(separate_count.z);
			if (temp >= separate_level) break;
		}
		area_unit.x = area_size.x / static_cast<float>(separate_count.x);
		area_unit.y = area_size.y / static_cast<float>(separate_count.y);
		area_unit.z = area_size.z / static_cast<float>(separate_count.z);
	}

	//	ブロック本体を先に全部構築（AABBだけ。三角形リストは後で埋める）
	block_datas.resize(static_cast<size_t>(separate_count.x) * separate_count.y * separate_count.z);

	//	三角形の事前計算
	struct triangle_cache
	{
		DirectX::XMFLOAT3 a, b, c;
		DirectX::XMVECTOR triangle_min, triangle_max;
	};
	std::vector<triangle_cache> triangle_caches(triangles.size());
	{
		auto p = positions.data();
		auto t = triangles.data();
		for (size_t index = 0; index < triangles.size(); ++index)
		{
			auto* tri = t + index;
			auto& tc = triangle_caches[index];
			tc.a = p[tri->a];
			tc.b = p[tri->b];
			tc.c = p[tri->c];
			auto A = DirectX::XMLoadFloat3(&tc.a);
			auto B = DirectX::XMLoadFloat3(&tc.b);
			auto C = DirectX::XMLoadFloat3(&tc.c);
			tc.triangle_min = DirectX::XMVectorMin(DirectX::XMVectorMin(A, B), C);
			tc.triangle_max = DirectX::XMVectorMax(DirectX::XMVectorMax(A, B), C);
		}
	}

	{
		DirectX::XMFLOAT3 extent = area_unit;
		extent.x *= 0.5f; extent.y *= 0.5f; extent.z *= 0.5f;

		for (UINT z = 0; z < separate_count.z; ++z)
		{
			for (UINT y = 0; y < separate_count.y; ++y)
			{
				for (UINT x = 0; x < separate_count.x; ++x)
				{
					UINT block_index = x + y * separate_count.x + z * separate_count.x * separate_count.y;
					block_data& data = block_datas[block_index];
					data.center = area_min;
					data.center.x += area_unit.x * (static_cast<float>(x) + 0.5f);
					data.center.y += area_unit.y * (static_cast<float>(y) + 0.5f);
					data.center.z += area_unit.z * (static_cast<float>(z) + 0.5f);
					data.bounds = inner::axis_aligned_bounding_box(extent, data.center);
				}
			}
		}
	}

	// 三角形ごとに「重なりうるブロック範囲」だけを見る（B×T → 大幅削減）
	auto clamp_index = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };

	auto process_range = [&](size_t begin, size_t end, std::vector<std::pair<UINT, UINT>>& out /* block_index, tri_index */)
	{
		auto t = triangles.data();
		for (size_t index = begin; index < end; ++index)
		{
			auto& tc = triangle_caches[index];

			DirectX::XMFLOAT3 tmin, tmax;
			DirectX::XMStoreFloat3(&tmin, tc.triangle_min);
			DirectX::XMStoreFloat3(&tmax, tc.triangle_max);

			int x0 = clamp_index(static_cast<int>((tmin.x - area_min.x) / area_unit.x) - 1, 0, static_cast<int>(separate_count.x) - 1);
			int x1 = clamp_index(static_cast<int>((tmax.x - area_min.x) / area_unit.x) + 1, 0, static_cast<int>(separate_count.x) - 1);
			int y0 = clamp_index(static_cast<int>((tmin.y - area_min.y) / area_unit.y) - 1, 0, static_cast<int>(separate_count.y) - 1);
			int y1 = clamp_index(static_cast<int>((tmax.y - area_min.y) / area_unit.y) + 1, 0, static_cast<int>(separate_count.y) - 1);
			int z0 = clamp_index(static_cast<int>((tmin.z - area_min.z) / area_unit.z) - 1, 0, static_cast<int>(separate_count.z) - 1);
			int z1 = clamp_index(static_cast<int>((tmax.z - area_min.z) / area_unit.z) + 1, 0, static_cast<int>(separate_count.z) - 1);

			auto* tri = t + index;
			for (int z = z0; z <= z1; ++z)
			{
				for (int y = y0; y <= y1; ++y)
				{
					for (int x = x0; x <= x1; ++x)
					{
						UINT block_index = static_cast<UINT>(x) +
											static_cast<UINT>(y) * separate_count.x +
											static_cast<UINT>(z) * separate_count.x * separate_count.y;

						block_data& data = block_datas[block_index];

						DirectX::XMVECTOR BoxMin, BoxMax;
						data.bounds.include_corner(BoxMin, BoxMax);

						auto Disjoint = DirectX::XMVectorOrInt(
							DirectX::XMVectorGreater(tc.triangle_min, BoxMax),
							DirectX::XMVectorGreater(BoxMin, tc.triangle_max));
						if (DirectX::Internal::XMVector3AnyTrue(Disjoint))
							continue;

						if (triangle_aabb(tc.a, tc.b, tc.c, tri->normal, data.bounds))
							out.emplace_back(block_index, static_cast<UINT>(index));
					}
				}
			}
		}
	};

	size_t triangle_count = triangles.size();
	if (triangle_count == 0)
		return;

	UINT thread_count = std::max(1u, std::min(std::thread::hardware_concurrency(), 16u));
	thread_count = static_cast<UINT>(std::min<size_t>(thread_count, triangle_count));

	std::vector<std::vector<std::pair<UINT, UINT>>> results(thread_count);
	{
		std::vector<std::thread> workers;
		size_t chunk = (triangle_count + thread_count - 1) / thread_count;
		for (UINT i = 0; i < thread_count; ++i)
		{
			size_t begin = i * chunk;
			size_t end = std::min(triangle_count, begin + chunk);
			if (begin >= end)
				continue;
			workers.emplace_back(process_range, begin, end, std::ref(results[i]));
		}
		for (auto& w : workers)
		{
			w.join();
		}
	}

	//	マージ
	std::vector<size_t> counts(block_datas.size(), 0);
	for (auto& r : results)
	{
		for (auto& p : r)
		{
			counts[p.first]++;
		}
	}
	for (size_t i = 0; i < block_datas.size(); ++i)
	{
		block_datas[i].triangles.reserve(counts[i]);
	}
	for (auto& r : results)
	{
		for (auto& p : r)
		{
			block_datas[p.first].triangles.emplace_back(p.second);
		}
	}
}

bounding_mesh::~bounding_mesh()
{
}

bool bounding_mesh::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(positions),
					CEREAL_NVP(triangles),

					CEREAL_NVP(area_min),
					CEREAL_NVP(area_max),
					CEREAL_NVP(area_size),
					CEREAL_NVP(area_unit),
					CEREAL_NVP(separate_count),
					CEREAL_NVP(block_datas));
		}
		catch (...)
		{
			DEBUG_ERROR("bounding_mesh serialize failed. %s\n", file_path.data());
			assert(!"bounding_mesh serialize failed.\n");
			return false;
		}

		return true;
	}
	return false;
}

bool bounding_mesh::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(positions),
					CEREAL_NVP(triangles),

					CEREAL_NVP(area_min),
					CEREAL_NVP(area_max),
					CEREAL_NVP(area_size),
					CEREAL_NVP(area_unit),
					CEREAL_NVP(separate_count),
					CEREAL_NVP(block_datas));
		}
		catch (...)
		{
			DEBUG_ERROR("bounding_mesh deserialize failed. %s\n", file_path.data());
			assert(!"bounding_mesh deserialize failed.\n");
			return false;
		}

		//	インデックスブロック内部のバウンディングボックス生成
		DirectX::XMFLOAT3 extent;
		extent.x = area_unit.x * 0.5f;
		extent.y = area_unit.y * 0.5f;
		extent.z = area_unit.z * 0.5f;
		for (auto& node : block_datas)
		{
			node.bounds = inner::axis_aligned_bounding_box(extent, node.center);
		}

		//	事前計算
		auto p = positions.data();
		for (auto& triangle : triangles)
		{
			auto A = DirectX::XMLoadFloat3(p  + triangle.a);
			auto B = DirectX::XMLoadFloat3(p  + triangle.b);
			auto C = DirectX::XMLoadFloat3(p  + triangle.c);
			auto AB = DirectX::XMVectorSubtract(B, A);
			auto BC = DirectX::XMVectorSubtract(C, B);
			auto CA = DirectX::XMVectorSubtract(A, C);
			DirectX::XMStoreFloat3(&triangle.ab, AB);
			DirectX::XMStoreFloat3(&triangle.bc, BC);
			DirectX::XMStoreFloat3(&triangle.ca, CA);

			auto Normal = DirectX::XMVector3Cross(AB, BC);
			DirectX::XMStoreFloat3(&triangle.normal, DirectX::XMVector3Normalize(Normal));

			auto Min = DirectX::XMVectorMin(A, DirectX::XMVectorMin(B, C));
			auto Max = DirectX::XMVectorMax(A, DirectX::XMVectorMax(B, C));
			DirectX::XMStoreFloat3(&triangle.min, Min);
			DirectX::XMStoreFloat3(&triangle.max, Max);
			triangle.lab = collision::line(collision::line::types::segment, *(p  + triangle.a), *(p  + triangle.b));
			triangle.lbc = collision::line(collision::line::types::segment, *(p  + triangle.b), *(p  + triangle.c));
			triangle.lca = collision::line(collision::line::types::segment, *(p  + triangle.c), *(p  + triangle.a));
		}
		return true;
	}
	OutputDebugStringA(file_path.data());
	assert(!"material deserialize failed.\n");
	return false;
}

bool bounding_mesh::intersect_point_inner(const DirectX::XMFLOAT3& point, float& nearlest, const std::vector<size_t>& triangle_indices) const
{
	if (triangle_indices.empty())
		return false;

	DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&point);
	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	nearlest = FLT_MAX;
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];

		DirectX::XMVECTOR A = DirectX::XMLoadFloat3(p + triangle->a);
		DirectX::XMVECTOR B = DirectX::XMLoadFloat3(p + triangle->b);
		DirectX::XMVECTOR C = DirectX::XMLoadFloat3(p + triangle->c);
		DirectX::XMVECTOR AB = DirectX::XMLoadFloat3(&triangle->ab);
		DirectX::XMVECTOR BC = DirectX::XMLoadFloat3(&triangle->bc);
		DirectX::XMVECTOR CA = DirectX::XMLoadFloat3(&triangle->ca);
		DirectX::XMVECTOR AC = DirectX::XMVectorNegate(CA);

		DirectX::XMVECTOR NP;
		calculate_nearlest_point(P, A, B, C, AB, BC, AC, NP);
		float t = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(NP, P)));
		if (t < nearlest)
			nearlest = t;
	}
	return nearlest < FLT_EPSILON;
}

bool bounding_mesh::intersect_point(const DirectX::XMFLOAT3& point) const
{
	DirectX::XMMATRIX InverseTransformMatrix = DirectX::XMMatrixInverse(nullptr, get_transform_matrix());

	DirectX::XMFLOAT3 local_point;
	DirectX::XMStoreFloat3(&local_point, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point), InverseTransformMatrix));

	float nearlest = FLT_MAX;

	auto index = calculate_array_index_from_point(local_point, area_min, area_unit, separate_count);
	if (index >= block_datas.size())
		return false;

	auto& node = block_datas[index];
	if (node.bounds.intersect_point(local_point))
	{
		if (intersect_point_inner(local_point, nearlest, node.triangles))
			return true;
	}
	return false;
}

bool bounding_mesh::intersect_segment_inner(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	auto Start	= DirectX::XMLoadFloat3(&start);
	auto End = DirectX::XMLoadFloat3(&end);
	auto Vec = DirectX::XMVectorSubtract(End, Start);
	auto Length = DirectX::XMVector3Length(Vec);
	auto Dir = DirectX::XMVectorDivide(Vec, Length);

	auto neart = DirectX::XMVectorGetX(Length);

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	const size_t triangle_count = triangle_indices.size();

	bool hit = false;
	DirectX::XMVECTOR HitPosition, HitNormal, Cross;
	for (size_t index = 0; index < triangle_count; ++index)
	{
		auto triangle = t[ti[index]];
		DirectX::XMVECTOR Normal = DirectX::XMLoadFloat3(&triangle.normal);
		DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(Normal, Dir);
		if (DirectX::XMVectorGetX(Dot) >= 0)
			continue;

		DirectX::XMVECTOR A = DirectX::XMLoadFloat3(p + triangle.a);

		DirectX::XMVECTOR V = DirectX::XMVectorSubtract(A, Start);
		DirectX::XMVECTOR T = DirectX::XMVectorDivide(DirectX::XMVector3Dot(Normal, V), Dot);

		float t = DirectX::XMVectorGetX(T);
		if (t < 0 || t > neart)
			continue;

		DirectX::XMVECTOR IP = DirectX::XMVectorMultiplyAdd(Dir, T, Start);

		V = DirectX::XMVectorSubtract(A, IP);
		Cross = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(A, IP), DirectX::XMLoadFloat3(&triangle.ab));
		Dot = DirectX::XMVector3Dot(Cross, Normal);
		if (DirectX::XMVectorGetX(Dot) < 0)
			continue;

		DirectX::XMVECTOR B = DirectX::XMLoadFloat3(p + triangle.b);
		Cross = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(B, IP), DirectX::XMLoadFloat3(&triangle.bc));
		Dot = DirectX::XMVector3Dot(Cross, Normal);
		if (DirectX::XMVectorGetX(Dot) < 0)
			continue;

		DirectX::XMVECTOR C = DirectX::XMLoadFloat3(p + triangle.c);
		Cross = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(C, IP), DirectX::XMLoadFloat3(&triangle.ca));
		Dot = DirectX::XMVector3Dot(Cross, Normal);
		if (DirectX::XMVectorGetX(Dot) < 0)
			continue;

		neart = t;
		HitPosition = IP;
		HitNormal = Normal;
		hit = true;
	}
	if (hit && intersect)
	{
		intersect_data::infomation infomation;
		DirectX::XMStoreFloat3(&infomation.point, HitPosition);
		DirectX::XMStoreFloat3(&infomation.normal, HitNormal);
		infomation.length = neart;
		intersect->add(infomation);
	}
	return	hit;
}

bool bounding_mesh::intersect_segment_inner_moller(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	auto Start	= DirectX::XMLoadFloat3(&start);
	auto End = DirectX::XMLoadFloat3(&end);
	auto Vec = DirectX::XMVectorSubtract(End, Start);
	auto Length = DirectX::XMVector3Length(Vec);
	auto Dir = DirectX::XMVectorDivide(Vec, Length);
	auto neart = DirectX::XMVectorGetX(Length);

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	const size_t triangle_count = triangle_indices.size();

	bool hit = false;
	DirectX::XMFLOAT3 hit_normal;
	DirectX::XMVECTOR HitPosition;
	for (size_t index = 0; index < triangle_count; ++index)
	{
		auto triangle = t[ti[index]];

		DirectX::XMVECTOR A = DirectX::XMLoadFloat3(p + triangle.a);
		DirectX::XMVECTOR e1 = DirectX::XMLoadFloat3(&triangle.ab);				//	e1 = B - A
		DirectX::XMVECTOR e2 = DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&triangle.ca));	//	e2 = C - A (= -CA)

		DirectX::XMVECTOR alpha = DirectX::XMVector3Cross(Dir, e2);
		float det = DirectX::XMVectorGetX(DirectX::XMVector3Dot(e1, alpha));

		if (det <= 1e-6f)
			continue;

		float inv_det = 1.0f / det;
		DirectX::XMVECTOR r = DirectX::XMVectorSubtract(Start, A);

		float u = DirectX::XMVectorGetX(DirectX::XMVector3Dot(alpha, r)) * inv_det;
		if (u < 0.0f || u > 1.0f)
			continue;

		DirectX::XMVECTOR beta = DirectX::XMVector3Cross(r, e1);

		float v = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Dir, beta)) * inv_det;
		if (v < 0.0f || u + v > 1.0f)
			continue;

		float tt = DirectX::XMVectorGetX(DirectX::XMVector3Dot(e2, beta)) * inv_det;
		if (tt < 0.0f || tt > neart)
			continue;

		neart = tt;
		HitPosition = DirectX::XMVectorMultiplyAdd(Dir, DirectX::XMVectorReplicate(tt), Start);
		hit_normal = triangle.normal;
		hit = true;
	}

	if (hit && intersect)
	{
		intersect_data::infomation infomation;
		DirectX::XMStoreFloat3(&infomation.point, HitPosition);
		infomation.normal = hit_normal;
		infomation.length = neart;
		intersect->add(infomation);
	}
	return	hit;
}

bool bounding_mesh::intersect_segment(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, intersect_data* intersect) const
{
	DirectX::XMMATRIX TransformMatrix = get_transform_matrix();
	DirectX::XMMATRIX InverseTransformMatrix = DirectX::XMMatrixInverse(nullptr, TransformMatrix);

	DirectX::XMFLOAT3 local_start, local_end;
	DirectX::XMStoreFloat3(&local_start, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&start), InverseTransformMatrix));
	DirectX::XMStoreFloat3(&local_end,   DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&end),   InverseTransformMatrix));

	DirectX::XMFLOAT3 min = local_start, max = local_start;
	min.x = std::min(min.x, local_end.x);
	min.y = std::min(min.y, local_end.y);
	min.z = std::min(min.z, local_end.z);
	max.x = std::max(max.x, local_end.x);
	max.y = std::max(max.y, local_end.y);
	max.z = std::max(max.z, local_end.z);
	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
	if (list.empty())
		return false;

	bool hit = false;
	intersect_data data;
	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (node.triangles.empty())
			continue;

		if (!node.bounds.intersect_segment(local_start, local_end))
			continue;

		if (intersect)
		{
			data.clear();
			if (intersect_segment_inner_moller(local_start, local_end, node.triangles, &data))
			{
				if (intersect->size() == 0)
				{
					*intersect = data;
				}
				else
				{
					if (data[0].length < (*intersect)[0].length)
					{
						*intersect = data;
					}
				}
				hit = true;
			}
		}
		else
		{
			if (intersect_segment_inner_moller(local_start, local_end, node.triangles))
				return true;
		}
	}

	//	ローカル空間からワールド空間へ変換
	if (hit && intersect)
	{
		for (size_t i = 0; i < intersect->size(); ++i)
		{
			auto& info = (*intersect)[i];
			DirectX::XMStoreFloat3(&info.point,
				DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&info.point), TransformMatrix));
			DirectX::XMStoreFloat3(&info.normal,
				DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&info.normal), TransformMatrix)));
		}
	}
	return hit;
}

bool bounding_mesh::intersect_sphere_inner(const DirectX::XMFLOAT3& point, float radius, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&point);
	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	DirectX::XMVECTOR NealestPoint = {};
	float nearlest = FLT_MAX;
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];
		DirectX::XMVECTOR A = DirectX::XMLoadFloat3(p + triangle->a);
		DirectX::XMVECTOR B = DirectX::XMLoadFloat3(p + triangle->b);
		DirectX::XMVECTOR C = DirectX::XMLoadFloat3(p + triangle->c);
		DirectX::XMVECTOR AB = DirectX::XMLoadFloat3(&triangle->ab);
		DirectX::XMVECTOR BC = DirectX::XMLoadFloat3(&triangle->bc);
		DirectX::XMVECTOR CA = DirectX::XMLoadFloat3(&triangle->ca);
		DirectX::XMVECTOR AC = DirectX::XMVectorNegate(CA);

		DirectX::XMVECTOR NP;
		calculate_nearlest_point(P, A, B, C, AB, BC, AC, NP);
		float t = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(NP, P)));
		if (t < nearlest)
		{
			nearlest = t;
			NealestPoint = NP;
		}
	}
	nearlest = sqrtf(nearlest);
	nearlest -= radius;
	if (nearlest <= 0.0f && intersect)
	{
		P = DirectX::XMLoadFloat3(&point);
		DirectX::XMVECTOR Vector = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(NealestPoint, P));
		DirectX::XMFLOAT3 p, n;
		DirectX::XMStoreFloat3(&p, NealestPoint);
		DirectX::XMStoreFloat3(&n, Vector);
		intersect->add(p, n, fabsf(nearlest));
	}
	return nearlest <= 0.0f;
}

bool bounding_mesh::intersect_sphere(const DirectX::XMFLOAT3& point, float radius, intersect_data* intersect) const
{
	DirectX::XMMATRIX TransformMatrix = get_transform_matrix();
	DirectX::XMMATRIX InverseTransformMatrix = DirectX::XMMatrixInverse(nullptr, TransformMatrix);

	DirectX::XMFLOAT3 local_point;
	DirectX::XMStoreFloat3(&local_point, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point), InverseTransformMatrix));

	DirectX::XMFLOAT3 min, max;
	min.x = local_point.x - radius;
	min.y = local_point.y - radius;
	min.z = local_point.z - radius;
	max.x = local_point.x + radius;
	max.y = local_point.y + radius;
	max.z = local_point.z + radius;
	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);

	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (intersect_sphere_inner(local_point, radius, node.triangles, intersect))
		{
			if (intersect && intersect->size() > 0)
			{
				//	ワールドへ変換
				auto& info = (*intersect)[intersect->size() - 1];
				DirectX::XMStoreFloat3(&info.point,
					DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&info.point), TransformMatrix));
				DirectX::XMStoreFloat3(&info.normal,
					DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&info.normal), TransformMatrix)));
			}
			return true;
		}
	}
	return false;
}

bool bounding_mesh::intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect) const
{
	return	intersect_sphere(sphere.get_position(), sphere.get_radius(), intersect);
}

bool bounding_mesh::intersect_capsule_inner(const bounding_capsule& local_capsule, DirectX::XMVECTOR CapsuleMin, DirectX::XMVECTOR CapsuleMax, DirectX::XMMATRIX TransformMatrix, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	auto local_capsule_line = local_capsule.get_line();
	auto radius = local_capsule.get_radius();
	auto LineA = DirectX::XMLoadFloat3(&local_capsule_line[0]);
	auto LineB = DirectX::XMLoadFloat3(&local_capsule_line[1]);

	DirectX::XMFLOAT3 nearlest_point_segment = {}, nearlest_point_triangle = {};
	bool nearlest_point_edghFlg = false;
	float nearlest = FLT_MAX;
	size_t nealest_face_index = -1;
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];

		//	ボックス判定
		auto Min = DirectX::XMLoadFloat3(&triangle->min);
		auto Max = DirectX::XMLoadFloat3(&triangle->max);
		DirectX::XMVECTOR Disjoint = DirectX::XMVectorOrInt(DirectX::XMVectorGreater(CapsuleMin, Max),
															DirectX::XMVectorGreater(Min, CapsuleMax));
		if (DirectX::Internal::XMVector3AnyTrue(Disjoint))
			continue;

		//	平面距離判定
		if (nearlest < FLT_MAX)
		{
			auto TriA = DirectX::XMLoadFloat3(p + triangle->a);
			auto Normal = DirectX::XMLoadFloat3(&triangle->normal);

			float dA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, DirectX::XMVectorSubtract(LineA, TriA)));
			float dB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, DirectX::XMVectorSubtract(LineB, TriA)));

			if (dA * dB > 0.0f)
			{
				if (std::min(fabsf(dA), fabsf(dB)) >= nearlest)
					continue;
			}
		}

		//	距離算出
		DirectX::XMFLOAT3 near_point_segment, near_point_triangle;
		bool near_point_edghFlg;
		float t = calculate_segment_distance(local_capsule_line, *triangle, near_point_segment, near_point_triangle, near_point_edghFlg);
		if (t < nearlest)
		{
			nearlest = t;
			nealest_face_index = index;
			nearlest_point_segment = near_point_segment;
			nearlest_point_triangle = near_point_triangle;
			nearlest_point_edghFlg = near_point_edghFlg;
		}
	}

	if (nearlest < radius && intersect)
	{
		auto triangle = t + nealest_face_index;
		DirectX::XMVECTOR TriangleA = DirectX::XMLoadFloat3(p + triangle->a);
		DirectX::XMVECTOR TriangleB = DirectX::XMLoadFloat3(p + triangle->b);
		DirectX::XMVECTOR TriangleC = DirectX::XMLoadFloat3(p + triangle->c);
		DirectX::XMVECTOR PointA = DirectX::XMLoadFloat3(&nearlest_point_segment);
		DirectX::XMVECTOR PointB = DirectX::XMLoadFloat3(&nearlest_point_triangle);

		DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&local_capsule.get_line()[0]);
		DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&local_capsule.get_line()[1]);
		DirectX::XMVECTOR Direction = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(Position, End));

		collision::intersect_data::infomation infomation;
		infomation.point = nearlest_point_segment;
		infomation.point2 = nearlest_point_triangle;
		DirectX::XMVECTOR Vector = DirectX::XMVectorSubtract(PointA, PointB);

		//	最短点が面上の場合
		DirectX::XMVECTOR Normal = DirectX::XMLoadFloat3(&triangle->normal);

		if (!nearlest_point_edghFlg)
		{
			//	線分が三角形を貫通している場合
			if (nearlest == 0)
			{
				//	両端の表裏判定
				float d = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, Direction));
				DirectX::XMVECTOR BackPoint = d > 0 ? Position : End;

				//	裏側の点から交点へのベクトルを算出し、カプセルのポジションに加算
				//	さらに法線の単位ベクトルにカプセルの半径を掛けたベクトルを用意し、カプセルのポジションに加算する
				Vector = DirectX::XMVectorAdd(DirectX::XMVectorSubtract(PointB, BackPoint), DirectX::XMVectorScale(Normal, radius));

				infomation.length = DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
				Normal = DirectX::XMVector3Normalize(Vector);
			}
			else
			{
				infomation.length = radius - DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
				Normal = DirectX::XMVector3Normalize(Vector);
			}
		}
		else
		{
			//	最短点が辺上の場合
			infomation.length = radius - nearlest;
		//	infomation.length = radius - DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
			Normal = DirectX::XMVector3Normalize(Vector);
		}

		//	法線が手に入らなかった場合は三角形の面を納入
		if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, Normal)) <= FLT_EPSILON)
		{
			Normal = DirectX::XMVector3Normalize(Vector);
		}

		//	法線を反転しておく
		DirectX::XMStoreFloat3(&infomation.normal, DirectX::XMVectorNegate(Normal));

		DirectX::XMStoreFloat3(&infomation.point, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&infomation.point), TransformMatrix));
		DirectX::XMStoreFloat3(&infomation.point2, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&infomation.point2), TransformMatrix));
		DirectX::XMStoreFloat3(&infomation.normal, DirectX::XMVector3TransformNormal(Normal, TransformMatrix));
		intersect->add(infomation);
	}
	return nearlest < radius;
}

bool bounding_mesh::intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect) const
{
	DirectX::XMFLOAT3 min, max;
	capsule.include_corner(min, max);
	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
	if (list.empty())
		return false;

	//	ローカル空間に変換
	bounding_capsule local_capsule = capsule;
	DirectX::XMMATRIX TransformMatrix = get_transform_matrix();
	{
		DirectX::XMMATRIX CopyTransformMatrix = local_capsule.get_transform_matrix();
		DirectX::XMMATRIX InverseTransformMatrix = DirectX::XMMatrixInverse(nullptr, TransformMatrix);
		DirectX::XMVECTOR Scale, Rotation, Translation;
		DirectX::XMMatrixDecompose(&Scale, &Rotation, &Translation, CopyTransformMatrix * InverseTransformMatrix);
		DirectX::XMFLOAT4 rotation;
		DirectX::XMFLOAT3 translation;
		DirectX::XMStoreFloat4(&rotation, Rotation);
		DirectX::XMStoreFloat3(&translation, Translation);
		local_capsule.set_rotation(rotation);
		local_capsule.set_position(translation);
	}

	DirectX::XMVECTOR Min, Max;
	inner::axis_aligned_bounding_box capsule_box;
	{
		DirectX::XMFLOAT3 min, max;
		local_capsule.include_corner(min, max);
		max.x = (max.x - min.x) * 0.5f;
		max.y = (max.y - min.y) * 0.5f;
		max.z = (max.z - min.z) * 0.5f;
		min.x += max.x;
		min.y += max.y;
		min.z += max.z;
		Min = DirectX::XMLoadFloat3(&min);
		Max = DirectX::XMLoadFloat3(&max);
		capsule_box = inner::axis_aligned_bounding_box(max, min);
	}

	bool hit = false;
	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (node.triangles.empty())
			continue;

		if (!node.bounds.intersect_capsule(capsule))
			continue;

		if (intersect)
		{
			intersect_data data;
			if (intersect_capsule_inner(local_capsule, Min, Max, TransformMatrix, node.triangles, &data))
			{
				if (intersect->size() == 0)
				{
					*intersect = data;
				}
				else
				{
					if (data[0].length < (*intersect)[0].length)
					{
						*intersect = data;
					}
				}
				hit = true;
			}
		}
		else
		{
			if (intersect_capsule_inner(local_capsule, Min, Max, TransformMatrix, node.triangles, intersect))
				return true;
		}
	}
	return hit;
}

bool bounding_mesh::intersect_box_inner(DirectX::XMVECTOR Center, DirectX::XMVECTOR AxisX, DirectX::XMVECTOR AxisY, DirectX::XMVECTOR AxisZ,
										 const DirectX::XMFLOAT3& half_extent, DirectX::XMVECTOR BoxMin, DirectX::XMVECTOR BoxMax,
										 const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();

	inner::axis_aligned_bounding_box local_box(half_extent, DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f});

	auto to_box_space = [&](DirectX::XMVECTOR V)
	{
		DirectX::XMVECTOR D = DirectX::XMVectorSubtract(V, Center);
		DirectX::XMFLOAT3 result;
		result.x = DirectX::XMVectorGetX(DirectX::XMVector3Dot(D, AxisX));
		result.y = DirectX::XMVectorGetX(DirectX::XMVector3Dot(D, AxisY));
		result.z = DirectX::XMVectorGetX(DirectX::XMVector3Dot(D, AxisZ));
		return result;
	};

	bool hit = false;
	float nearlest = FLT_MAX;
	DirectX::XMVECTOR NearlestPoint = {};
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];

		DirectX::XMVECTOR TriMin = DirectX::XMLoadFloat3(&triangle->min);
		DirectX::XMVECTOR TriMax = DirectX::XMLoadFloat3(&triangle->max);
		DirectX::XMVECTOR Disjoint = DirectX::XMVectorOrInt(DirectX::XMVectorGreater(TriMin, BoxMax),
															 DirectX::XMVectorGreater(BoxMin, TriMax));
		if (DirectX::Internal::XMVector3AnyTrue(Disjoint))
			continue;

		DirectX::XMVECTOR A = DirectX::XMLoadFloat3(p + triangle->a);
		DirectX::XMVECTOR B = DirectX::XMLoadFloat3(p + triangle->b);
		DirectX::XMVECTOR C = DirectX::XMLoadFloat3(p + triangle->c);
		DirectX::XMVECTOR Normal = DirectX::XMLoadFloat3(&triangle->normal);

		DirectX::XMFLOAT3 la = to_box_space(A);
		DirectX::XMFLOAT3 lb = to_box_space(B);
		DirectX::XMFLOAT3 lc = to_box_space(C);
		DirectX::XMFLOAT3 ln;
		ln.x = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, AxisX));
		ln.y = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, AxisY));
		ln.z = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, AxisZ));

		if (!triangle_aabb(la, lb, lc, ln, local_box))
			continue;

		hit = true;

		if (!intersect)
			continue;

		DirectX::XMVECTOR AB = DirectX::XMLoadFloat3(&triangle->ab);
		DirectX::XMVECTOR BC = DirectX::XMLoadFloat3(&triangle->bc);
		DirectX::XMVECTOR CA = DirectX::XMLoadFloat3(&triangle->ca);
		DirectX::XMVECTOR AC = DirectX::XMVectorNegate(CA);

		DirectX::XMVECTOR NP;
		calculate_nearlest_point(Center, A, B, C, AB, BC, AC, NP);

		float distance_square = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(NP, Center)));
		if (distance_square < nearlest)
		{
			nearlest = distance_square;
			NearlestPoint = NP;
		}
	}

	if (hit && intersect)
	{
		DirectX::XMVECTOR Vector = DirectX::XMVectorSubtract(NearlestPoint, Center);
		float distance = sqrtf(nearlest);
		DirectX::XMVECTOR Direction = (distance > FLT_EPSILON) ? DirectX::XMVector3Normalize(Vector) : AxisY;

		float support = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(AxisX, Direction))) * half_extent.x
					   + fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(AxisY, Direction))) * half_extent.y
					   + fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(AxisZ, Direction))) * half_extent.z;

		DirectX::XMFLOAT3 point, normal;
		DirectX::XMStoreFloat3(&point, NearlestPoint);
		DirectX::XMStoreFloat3(&normal, Direction);
		intersect->add(point, normal, fabsf(support - distance));
	}

	return hit;
}

bool bounding_mesh::intersect_box(const bounding_box& box, intersect_data* intersect) const
{
	//	@remainder	未検証
	DirectX::XMMATRIX TransformMatrix = get_transform_matrix();
	DirectX::XMMATRIX InverseTransformMatrix = DirectX::XMMatrixInverse(nullptr, TransformMatrix);

	DirectX::XMVECTOR Scale, Rotation, Translation;
	DirectX::XMMatrixDecompose(&Scale, &Rotation, &Translation, box.get_transform_matrix() * InverseTransformMatrix);

	DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationQuaternion(Rotation);
	DirectX::XMVECTOR AxisX = DirectX::XMVector3Normalize(RotationMatrix.r[0]);
	DirectX::XMVECTOR AxisY = DirectX::XMVector3Normalize(RotationMatrix.r[1]);
	DirectX::XMVECTOR AxisZ = DirectX::XMVector3Normalize(RotationMatrix.r[2]);
	DirectX::XMVECTOR Center = Translation;

	const DirectX::XMFLOAT3& half_extent = box.get_local_scale();

	DirectX::XMVECTOR Extent = DirectX::XMVectorAdd(
		DirectX::XMVectorAdd(
			DirectX::XMVectorScale(DirectX::XMVectorAbs(AxisX), half_extent.x),
			DirectX::XMVectorScale(DirectX::XMVectorAbs(AxisY), half_extent.y)),
		DirectX::XMVectorScale(DirectX::XMVectorAbs(AxisZ), half_extent.z));
	DirectX::XMVECTOR BoxMin = DirectX::XMVectorSubtract(Center, Extent);
	DirectX::XMVECTOR BoxMax = DirectX::XMVectorAdd(Center, Extent);

	DirectX::XMFLOAT3 min, max;
	DirectX::XMStoreFloat3(&min, BoxMin);
	DirectX::XMStoreFloat3(&max, BoxMax);

	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
	if (list.empty())
		return false;

	bool hit = false;
	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (node.triangles.empty())
			continue;

		if (!node.bounds.intersect_box(box))
			continue;

		if (intersect)
		{
			intersect_data data;
			if (intersect_box_inner(Center, AxisX, AxisY, AxisZ, half_extent, BoxMin, BoxMax, node.triangles, &data))
			{
				if (intersect->size() == 0)
				{
					*intersect = data;
				}
				else
				{
					if (data[0].length < (*intersect)[0].length)
					{
						*intersect = data;
					}
				}
				hit = true;
			}
		}
		else
		{
			if (intersect_box_inner(Center, AxisX, AxisY, AxisZ, half_extent, BoxMin, BoxMax, node.triangles, nullptr))
				return true;
		}
	}
	return hit;
}

bool bounding_mesh::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	//	未対応
//	DEBUG_LOG("not corespound mesh vs mesh\n");
	return false;
}

bool bounding_mesh::intersect_sphere_cast_inner(const collision::line& sweep_line, float radius, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	//	各三角形について移動する球との交差を判定し、メッシュ内で最も移動距離の短い交点を採用する
	if (triangle_indices.empty())
		return false;

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();

	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&sweep_line[0]);
	DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&sweep_line[1]);
	DirectX::XMVECTOR SweepDirectionNormalize = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(End, Start));
	DirectX::XMVECTOR qp = DirectX::XMVectorSubtract(Start, End);

	//	掃引する球のAABB（三角形の粗い枝刈り用）
	DirectX::XMVECTOR RadiusVec = DirectX::XMVectorReplicate(radius);
	DirectX::XMVECTOR CastMin = DirectX::XMVectorSubtract(DirectX::XMVectorMin(Start, End), RadiusVec);
	DirectX::XMVECTOR CastMax = DirectX::XMVectorAdd(DirectX::XMVectorMax(Start, End), RadiusVec);

	bool hit_flag = false;
	float nearlest_distance = FLT_MAX;
	DirectX::XMVECTOR nearlest_triangle_pos = {};
	DirectX::XMVECTOR nearlest_normal = {};
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];

		DirectX::XMVECTOR Min = DirectX::XMLoadFloat3(&triangle->min);
		DirectX::XMVECTOR Max = DirectX::XMLoadFloat3(&triangle->max);
		DirectX::XMVECTOR Disjoint = DirectX::XMVectorOrInt(DirectX::XMVectorGreater(CastMin, Max),
															DirectX::XMVectorGreater(Min, CastMax));
		if (DirectX::Internal::XMVector3AnyTrue(Disjoint))
			continue;

		DirectX::XMVECTOR TriangleA = DirectX::XMLoadFloat3(p + triangle->a);
		DirectX::XMVECTOR TriangleB = DirectX::XMLoadFloat3(p + triangle->b);
		DirectX::XMVECTOR TriangleC = DirectX::XMLoadFloat3(p + triangle->c);
		DirectX::XMVECTOR EdgeAB = DirectX::XMLoadFloat3(&triangle->ab);
		DirectX::XMVECTOR EdgeBC = DirectX::XMLoadFloat3(&triangle->bc);
		DirectX::XMVECTOR EdgeCA = DirectX::XMLoadFloat3(&triangle->ca);
		DirectX::XMVECTOR EdgeAC = DirectX::XMVectorNegate(EdgeCA);

		//	開始位置ですでに球と三角形が交差している場合は、この三角形での判定を行わない
		DirectX::XMVECTOR NearestOnTriangle;
		calculate_nearlest_point(Start, TriangleA, TriangleB, TriangleC, EdgeAB, EdgeBC, EdgeAC, NearestOnTriangle);
		if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(NearestOnTriangle, Start))) <= radius * radius)
			continue;

		//	三角形の法線（正規化しない：後続の平面判定のスケールに用いるため）
		DirectX::XMVECTOR Norm = DirectX::XMVector3Cross(EdgeAB, EdgeAC);
		float d = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Norm, qp));

		bool triangle_hit = false;
		float hit_distance = 0.0f;
		DirectX::XMVECTOR hit_position = {};

		if (d >= 0.0f)	//	表側から接近している場合のみ判定する
		{
			//	面領域の判定（球半径分オフセットした三角形との交差）
			DirectX::XMVECTOR fix_vec = DirectX::XMVectorScale(DirectX::XMVector3Normalize(Norm), radius);
			DirectX::XMVECTOR ap = DirectX::XMVectorSubtract(Start, DirectX::XMVectorAdd(TriangleA, fix_vec));
			float plane_t = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Norm, ap));
			if (plane_t >= 0.0f && plane_t < d)
			{
				DirectX::XMVECTOR e = DirectX::XMVector3Cross(qp, ap);
				float v = DirectX::XMVectorGetX(DirectX::XMVector3Dot(EdgeAC, e));
				if (v >= 0.0f && v <= d)
				{
					float w = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(EdgeAB, e));
					if (w > 0.0f && v + w <= d)
					{
						DirectX::XMVECTOR cross_pos = DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(qp, -plane_t / d));
						hit_distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(cross_pos, Start)));
						hit_position = DirectX::XMVectorSubtract(cross_pos, fix_vec);
						triangle_hit = true;
					}
				}
			}
		}

		if (!triangle_hit)
		{
			//	面領域で交差しなかった場合は、頂点・辺のボロノイ領域で最短距離の交点を求める
			float bound_distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(qp));

			const DirectX::XMVECTOR verts[3] = { TriangleA, TriangleB, TriangleC };
			for (int v = 0; v < 3; ++v)
			{
				DirectX::XMVECTOR ray2sphere = DirectX::XMVectorSubtract(verts[v], Start);
				float projection = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ray2sphere, SweepDirectionNormalize));
				if (projection <= 0.0f)
					continue;

				float dist_sq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(ray2sphere)) - projection * projection;
				if (dist_sq < radius * radius)
				{
					float dist = projection - sqrtf(radius * radius - dist_sq);
					if (bound_distance > dist && dist >= 0)
					{
						bound_distance = dist;
						hit_distance = dist;
						hit_position = verts[v];
						triangle_hit = true;
					}
				}
			}

			struct
			{
				DirectX::XMVECTOR p0, p1;
			}
			const edges[3] =
			{
				{ TriangleA, TriangleB },
				{ TriangleA, TriangleC },
				{ TriangleB, TriangleC },
			};

			for (int e = 0; e < 3; ++e)
			{
				DirectX::XMVECTOR cyl_pos;
				float cyl_dist;
				if (intersect_ray_vs_cylinder(Start, End, edges[e].p0, edges[e].p1, radius, cyl_pos, cyl_dist))
				{
					if (bound_distance > cyl_dist)
					{
						DirectX::XMVECTOR edge_dir = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(edges[e].p1, edges[e].p0));
						DirectX::XMVECTOR vec0p = DirectX::XMVectorSubtract(cyl_pos, edges[e].p0);
						bound_distance = cyl_dist;
						hit_distance = cyl_dist;
						hit_position = DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Dot(vec0p, edge_dir), edge_dir, edges[e].p0);
						triangle_hit = true;
					}
				}
			}
		}

		if (triangle_hit && hit_distance < nearlest_distance)
		{
			nearlest_distance = hit_distance;
			nearlest_triangle_pos = hit_position;
			//nearlest_normal = DirectX::XMLoadFloat3(&triangle->normal);
			nearlest_normal = DirectX::XMVector3Normalize(Norm);
			hit_flag = true;
		}
	}

	if (intersect && hit_flag)
	{
		//	接触時点の球中心位置
		DirectX::XMVECTOR SphereCenterAtHit = DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(SweepDirectionNormalize, nearlest_distance));

		collision::intersect_data::infomation infomation;
		DirectX::XMStoreFloat3(&infomation.point, SphereCenterAtHit);		//	接触時点の球の中心位置
		DirectX::XMStoreFloat3(&infomation.point2, nearlest_triangle_pos);	//	三角形側の接触点
		infomation.length = nearlest_distance;								//	接触までの移動距離
		DirectX::XMStoreFloat3(&infomation.normal, nearlest_normal);		//	接触面の法線
		intersect->add(infomation);
	}
	return hit_flag;
}

bool bounding_mesh::intersect_sphere_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float radius, intersect_data* intersect) const
{
	DirectX::XMMATRIX TransformMatrix = get_transform_matrix();
	DirectX::XMMATRIX InverseTransformMatrix = DirectX::XMMatrixInverse(nullptr, TransformMatrix);

	DirectX::XMFLOAT3 local_start, local_end;
	DirectX::XMStoreFloat3(&local_start, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&start), InverseTransformMatrix));
	DirectX::XMStoreFloat3(&local_end,   DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&end),   InverseTransformMatrix));

	DirectX::XMFLOAT3 min, max;
	min.x = std::min(local_start.x, local_end.x) - radius;
	min.y = std::min(local_start.y, local_end.y) - radius;
	min.z = std::min(local_start.z, local_end.z) - radius;
	max.x = std::max(local_start.x, local_end.x) + radius;
	max.y = std::max(local_start.y, local_end.y) + radius;
	max.z = std::max(local_start.z, local_end.z) + radius;

	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
	if (list.empty())
		return false;

	collision::line sweep_line(collision::line::types::segment, local_start, local_end);

	bool hit = false;
	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (node.triangles.empty())
			continue;

		if (intersect)
		{
			intersect_data data;
			if (intersect_sphere_cast_inner(sweep_line, radius, node.triangles, &data))
			{
				if (intersect->size() == 0 || data[0].length < (*intersect)[0].length)
				{
					*intersect = data;
				}
				hit = true;
			}
		}
		else
		{
			if (intersect_sphere_cast_inner(sweep_line, radius, node.triangles, nullptr))
				return true;
		}
	}

	//	結果をローカル空間からワールド空間へ変換
	if (hit && intersect)
	{
		for (size_t i = 0; i < intersect->size(); ++i)
		{
			auto& info = (*intersect)[i];
			DirectX::XMStoreFloat3(&info.point,
				DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&info.point), TransformMatrix));
			DirectX::XMStoreFloat3(&info.point2,
				DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&info.point2), TransformMatrix));
			DirectX::XMStoreFloat3(&info.normal,
				DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&info.normal), TransformMatrix)));
		}
	}

	return hit;
}

void bounding_mesh::include_corner(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max) const
{
	min = area_min;
	max = area_max;
}

float bounding_mesh::calculate_segment_distance(const collision::line& line,
												const triangle& triangle,
												DirectX::XMFLOAT3& nearlest_point_segment,
												DirectX::XMFLOAT3& nearlest_point_triangle,
												bool& nearlest_point_edghFlg) const
{
	nearlest_point_edghFlg = false;

	//	線分と三角形の交差判定
	auto p = positions.data();
	DirectX::XMVECTOR PointA = DirectX::XMLoadFloat3(&line[0]);
	DirectX::XMVECTOR PointB = DirectX::XMLoadFloat3(&line[1]);
	DirectX::XMVECTOR TriangleA = DirectX::XMLoadFloat3(p + triangle.a);
	DirectX::XMVECTOR TriangleB = DirectX::XMLoadFloat3(p + triangle.b);
	DirectX::XMVECTOR TriangleC = DirectX::XMLoadFloat3(p + triangle.c);
	DirectX::XMVECTOR EdgeAB = DirectX::XMLoadFloat3(&triangle.ab);
	DirectX::XMVECTOR EdgeBC = DirectX::XMLoadFloat3(&triangle.bc);
	DirectX::XMVECTOR EdgeCA = DirectX::XMLoadFloat3(&triangle.ca);
	DirectX::XMVECTOR EdgeAC = DirectX::XMVectorNegate(EdgeCA);
	DirectX::XMVECTOR TriangleNormal = DirectX::XMLoadFloat3(&triangle.normal);

	DirectX::XMVECTOR RayDirection = DirectX::XMVectorSubtract(PointB, PointA);
	DirectX::XMVECTOR IntersectPoint;
	float distance;
	if (intersect_ray_moller(PointA, RayDirection,
					  TriangleA, TriangleB, TriangleC,
					  EdgeAB, EdgeCA,
					  IntersectPoint, distance))
	{
		DirectX::XMStoreFloat3(&nearlest_point_segment, IntersectPoint);
		DirectX::XMStoreFloat3(&nearlest_point_triangle, IntersectPoint);
		return 0.0f;
	}
#if 0	//	逆方向からの二回目は破棄。復活させる場合は delta > max_delta 判定が不要
	RayDirection = DirectX::XMVectorNegate(RayDirection);
	if (intersect_ray(PointB, RayDirection,
					  TriangleA, TriangleB, TriangleC,
					  EdgeAB, EdgeBC, EdgeCA, TriangleNormal,
					  IntersectPoint, distance))
	{
		DirectX::XMStoreFloat3(&nearlest_point_segment, IntersectPoint);
		DirectX::XMStoreFloat3(&nearlest_point_triangle, IntersectPoint);
		return 0.0f;
	}
#endif

	//	線分の各頂点に対して三角形の最近点を求める
	DirectX::XMVECTOR NearPosA, NearPosB;
	bool insideA = calculate_nearlest_point(PointA, TriangleA, TriangleB, TriangleC,
											EdgeAB, EdgeBC, EdgeAC, NearPosA);
	bool insideB = calculate_nearlest_point(PointB, TriangleA, TriangleB, TriangleC,
											EdgeAB, EdgeBC, EdgeAC, NearPosB);

	float nearlest_distance = FLT_MAX;
	if (insideA || insideB)
	{
		float distance_squareA, distance_squareB;
		distance_squareA = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(NearPosA, PointA)));
		distance_squareB = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(NearPosB, PointB)));

		if (distance_squareA < distance_squareB)
		{
			nearlest_distance = sqrtf(distance_squareA);
			DirectX::XMStoreFloat3(&nearlest_point_segment, PointA);
			DirectX::XMStoreFloat3(&nearlest_point_triangle, NearPosA);
		}
		else
		{
			nearlest_distance = sqrtf(distance_squareB);
			DirectX::XMStoreFloat3(&nearlest_point_segment, PointB);
			DirectX::XMStoreFloat3(&nearlest_point_triangle, NearPosB);
		}

		//	両端とも射影点が三角形内部
		if (insideA && insideB)
		{
			return nearlest_distance;
		}

		//	既にほぼ接触(距離0)なら辺判定は不要
		if (nearlest_distance <= 1e-6f)
		{
			return nearlest_distance;
		}
	}

	//	最近傍点は頂点と線分上に存在(カプセル用に必須)
	nearlest_point_edghFlg = true;
	DirectX::XMFLOAT3 a = *(p + triangle.a);
	DirectX::XMFLOAT3 b = *(p + triangle.b);
	DirectX::XMFLOAT3 c = *(p + triangle.c);

	//	三角形外部に線分（または端）が存在し、最近点が辺上になる場合、線分と三角形の３辺の距離を全て計測し比較する
	DirectX::XMFLOAT3 horizon1, horizon2;
	float delta1, delta2;

	float distance_edge = line.calculate_distance(horizon1, delta1, horizon2, delta2, triangle.lab);
	if (distance_edge < nearlest_distance)
	{
		nearlest_distance = distance_edge;
		DirectX::XMStoreFloat3(&nearlest_point_segment, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon1), delta1));
		DirectX::XMStoreFloat3(&nearlest_point_triangle, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon2), delta2));
	}

	distance_edge = line.calculate_distance(horizon1, delta1, horizon2, delta2, triangle.lbc);
	if (distance_edge < nearlest_distance)
	{
		nearlest_distance = distance_edge;
		DirectX::XMStoreFloat3(&nearlest_point_segment, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon1), delta1));
		DirectX::XMStoreFloat3(&nearlest_point_triangle, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon2), delta2));
	}

	distance_edge = line.calculate_distance(horizon1, delta1, horizon2, delta2, triangle.lca);
	if (distance_edge < nearlest_distance)
	{
		nearlest_distance = distance_edge;
		DirectX::XMStoreFloat3(&nearlest_point_segment, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon1), delta1));
		DirectX::XMStoreFloat3(&nearlest_point_triangle, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon2), delta2));
	}

	return nearlest_distance;
}

////////////////////////////////////////////////////////////////////////////////////////////////
bool collision::generate_bounding_mesh(bounding_mesh& out_mesh, model_resource_ptr resource, size_t index, generate_bounding_mesh_data data)
{
	auto& mesh = resource->get_meshes()[index];
	if (mesh.index_buffer_view.count() == 0)
		return false;

	auto& nodes = resource->get_nodes();
	std::function<DirectX::XMMATRIX(size_t index)> calculate_transform_matrix = [&](size_t node_index)
	{
		if (node_index == model_resource::invalid_id)
			return DirectX::XMMatrixIdentity();

		auto& node = nodes[node_index];
		DirectX::XMMATRIX transform;
		transform = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z)
				  * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation))
				  * DirectX::XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z);
		if (node.parent_index != model_resource::invalid_id)
			transform *= calculate_transform_matrix(node.parent_index);

		return transform;
	};

	DirectX::XMMATRIX local_transform_matrix = calculate_transform_matrix(mesh.node_index);

	auto it = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
	if (it == mesh.vertex_buffer_views.end())
		return false;

	std::vector<DirectX::XMFLOAT3> positions;
	std::vector<bounding_mesh::triangle> triangles;
	{
		auto p = reinterpret_cast<const DirectX::XMFLOAT3*>(it->second.row_data.data());
		for (UINT index = 0; index < it->second.count(); ++index)
		{
			DirectX::XMVECTOR P = DirectX::XMLoadFloat3(p + index);
			DirectX::XMStoreFloat3(&positions.emplace_back(), DirectX::XMVector3TransformCoord(P, local_transform_matrix));
		}
		auto i = reinterpret_cast<const UINT*>(mesh.index_buffer_view.row_data.data());
		UINT index1 = data.reverse_flag ? 2 : 0;
		UINT index2 = 1;
		UINT index3 = data.reverse_flag ? 0 : 2;
		for (UINT index = 0; index < mesh.index_buffer_view.count(); index += 3)
		{
			bounding_mesh::triangle& triangle = triangles.emplace_back();
			triangle.a = i[index + index1];
			triangle.b = i[index + index2];
			triangle.c = i[index + index3];

			//	事前計算
			auto A = DirectX::XMLoadFloat3(p  + triangle.a);
			auto B = DirectX::XMLoadFloat3(p  + triangle.b);
			auto C = DirectX::XMLoadFloat3(p  + triangle.c);
			auto AB = DirectX::XMVectorSubtract(B, A);
			auto BC = DirectX::XMVectorSubtract(C, B);
			auto CA = DirectX::XMVectorSubtract(A, C);
			DirectX::XMStoreFloat3(&triangle.ab, AB);
			DirectX::XMStoreFloat3(&triangle.bc, BC);
			DirectX::XMStoreFloat3(&triangle.ca, CA);

			auto Normal = DirectX::XMVector3Cross(AB, BC);
			DirectX::XMStoreFloat3(&triangle.normal, DirectX::XMVector3Normalize(Normal));

			auto Min = DirectX::XMVectorMin(A, DirectX::XMVectorMin(B, C));
			auto Max = DirectX::XMVectorMax(A, DirectX::XMVectorMax(B, C));
			DirectX::XMStoreFloat3(&triangle.min, Min);
			DirectX::XMStoreFloat3(&triangle.max, Max);
			triangle.lab = collision::line(collision::line::types::segment, *(p  + triangle.a), *(p  + triangle.b));
			triangle.lbc = collision::line(collision::line::types::segment, *(p  + triangle.b), *(p  + triangle.c));
			triangle.lca = collision::line(collision::line::types::segment, *(p  + triangle.c), *(p  + triangle.a));
		}
	}

	out_mesh = collision::bounding_mesh(positions, triangles, data.separate_level);

	{
		DirectX::XMVECTOR Scale, Rotation, Translation;
		DirectX::XMMatrixDecompose(&Scale, &Rotation, &Translation, data.WorldMatrix);

		DirectX::XMFLOAT4 rotation;
		DirectX::XMFLOAT3 translation;
		DirectX::XMStoreFloat4(&rotation, Rotation);
		DirectX::XMStoreFloat3(&translation, Translation);

		out_mesh.set_rotation(rotation);
		out_mesh.set_position(translation);
	}

	return true;
}
