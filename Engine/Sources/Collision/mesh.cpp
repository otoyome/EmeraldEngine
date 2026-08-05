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

#if 0
		// 1. 三角形のAABBとAABBの交差判定
		DirectX::XMFLOAT3 bmin, bmax;
		bmin.x = -box.get_local_scale().x;
		bmin.y = -box.get_local_scale().y;
		bmin.z = -box.get_local_scale().z;
		bmax = box.get_local_scale();
		
		DirectX::XMFLOAT3 tmin = v[0], tmax = v[0];
		for(int j = 1; j < 3; ++j){
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
#endif
		// 3. AABBの各軸方向とエッジのテスト
		DirectX::XMFLOAT3 e[3] = { {0, 0, 0}, {0, 0, 0}, {0, 0, 0} }; // AABBの各軸方向ベクトル
		DirectX::XMFLOAT3 f[3] = {}; // 三角形のエッジベクトル
		e[0].x = 1.0f;
		f[0].x = v[1].x - v[0].x;
		f[0].y = v[1].y - v[0].y;
		f[0].z = v[1].z - v[0].z;
		e[1].x = 1.0f;
		f[1].x = v[2].x - v[1].x;
		f[1].y = v[2].y - v[1].y;
		f[1].z = v[2].z - v[1].z;
		e[2].x = 1.0f;
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
	#if 01
		//	除算はここで1回だけ行う
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
	#else
		DirectX::XMFLOAT3 point = min, p;
		for (UINT x = 0; x < axis_count.x; ++x)
		{
			p.x = (point.x - area_min.x) / area_unit.x;
			UINT index_x = static_cast<UINT>(p.x);
			float reserve_y = point.y;
			for (UINT y = 0; y < axis_count.y; ++y)
			{
				p.y = (point.y - area_min.y) / area_unit.y;
				UINT index_y = static_cast<UINT>(p.y);
				UINT index_xy = index_x + index_y * separate_count.x;
				float reserve_z = point.z;
				for (UINT z = 0; z < axis_count.z; ++z)
				{
					p.z = (point.z - area_min.z) / area_unit.z;
					UINT index_z = static_cast<UINT>(p.z);
					UINT index = index_xy + index_z * square_separate_count;
					if (index < cube_separate_count)
					{
						order_list.emplace_back(index);
					}
					point.z += area_unit.z;
				}
				point.y += area_unit.y;
				point.z = reserve_z;
			}
			point.x += area_unit.x;
			point.y = reserve_y;
		}
	#endif
		return order_list;
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
	include_corner(area_min, area_max);
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
	{
		DirectX::XMFLOAT3 extent = area_unit;
		extent.x *= 0.5f; extent.y *= 0.5f; extent.z *= 0.5f;

		for (UINT z = 0; z < separate_count.z; ++z)
		for (UINT y = 0; y < separate_count.y; ++y)
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
	float nearlest = FLT_MAX;

	auto index = calculate_array_index_from_point(point, area_min, area_unit, separate_count);
	auto& node = block_datas[index];
	if (node.bounds.intersect_point(point))
	{
		if (intersect_point_inner(point, nearlest, node.triangles))
			return true;
	}
	return false;
}

bool bounding_mesh::intersect_segment_inner(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const std::vector<size_t>& triangle_indices, intersect_data* result) const
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
	if (hit && result)
	{
		intersect_data::infomation infomation;
		DirectX::XMStoreFloat3(&infomation.point, HitPosition);
		DirectX::XMStoreFloat3(&infomation.normal, HitNormal);
		infomation.length = neart;
		result->add(infomation);
	}
	return	hit;
}

bool bounding_mesh::intersect_segment(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, intersect_data* intersect) const
{
	DirectX::XMFLOAT3 min = start, max = start;
	min.x = std::min(min.x, end.x);
	min.y = std::min(min.y, end.y);
	min.z = std::min(min.z, end.z);
	max.x = std::max(max.x, end.x);
	max.y = std::max(max.y, end.y);
	max.z = std::max(max.z, end.z);
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

		if (!node.bounds.intersect_segment(start, end))
			continue;

		if (intersect)
		{
			data.clear();
			if (intersect_segment_inner(start, end, node.triangles, &data))
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
			if (intersect_segment_inner(start, end, node.triangles))
				return true;
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
	float nearlest = FLT_MAX;
	DirectX::XMFLOAT3 min, max;
	min.x = point.x - radius;
	min.y = point.y - radius;
	min.z = point.z - radius;
	max.x = point.x + radius;
	max.y = point.y + radius;
	max.z = point.z + radius;
	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (intersect_sphere_inner(point, radius, node.triangles, intersect))
		{
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

	#if 0	//	不要かな？
		if (nearlest < FLT_MAX)
		{
			DirectX::XMVECTOR TriA = DirectX::XMLoadFloat3(p + triangle->a);
			DirectX::XMVECTOR Normal = DirectX::XMLoadFloat3(&triangle->normal);
			DirectX::XMVECTOR LineA = DirectX::XMLoadFloat3(&local_capsule_line[0]);
			DirectX::XMVECTOR LineB = DirectX::XMLoadFloat3(&local_capsule_line[1]);

			float dA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, DirectX::XMVectorSubtract(LineA, TriA)));
			float dB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, DirectX::XMVectorSubtract(LineB, TriA)));

			//	線分が平面を跨いでいない(同じ側)場合のみ、平面距離を下限として使える
			if (dA * dB > 0.0f)
			{
				float plane_distance = std::min(fabsf(dA), fabsf(dB));
				if (plane_distance >= nearlest)
					continue;	//	これ以上調べても更新されない
			}
		}
	#endif

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
		if (node.bounds.intersect_capsule(capsule))
		{
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
	}
	return hit;
}

bool bounding_mesh::intersect_box(const bounding_box& box, intersect_data* intersect) const
{
	//	未対応
//	DEBUG_LOG("not corespound mesh vs box\n");
	return false;
}

bool bounding_mesh::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	//	未対応
//	DEBUG_LOG("not corespound mesh vs mesh\n");
	return false;
}

void bounding_mesh::include_corner(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max) const
{
	min = max = positions[0];
	for (auto& position : positions)
	{
		min.x	= std::min(position.x, min.x);
		min.y	= std::min(position.y, min.y);
		min.z	= std::min(position.z, min.z);
		max.x	= std::max(position.x, max.x);
		max.y	= std::max(position.y, max.y);
		max.z	= std::max(position.z, max.z);
	}
}

bool bounding_mesh::calculate_nearlest_point(DirectX::XMVECTOR P,
											 DirectX::XMVECTOR A, DirectX::XMVECTOR B, DirectX::XMVECTOR C,
											 DirectX::XMVECTOR EdgeAB, DirectX::XMVECTOR EdgeBC, DirectX::XMVECTOR EdgeAC,
											 DirectX::XMVECTOR& NearlestPoint) const
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

bool bounding_mesh::intersect_ray(DirectX::XMVECTOR Start,
								  DirectX::XMVECTOR Direction,
								  DirectX::XMVECTOR A,
								  DirectX::XMVECTOR B,
								  DirectX::XMVECTOR C,
								  DirectX::XMVECTOR EdgeAB,
								  DirectX::XMVECTOR EdgeBC,
								  DirectX::XMVECTOR EdgeCA,
								  DirectX::XMVECTOR Normal,
								  DirectX::XMVECTOR& InsertPoint,
								  float& distance) const
{
	float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Direction, Normal));
	if (dot >= 0.0f)
		return	false;

	auto p = positions.data();

	float dot2 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSubtract(A, Start), Normal));
	float delta = (dot2 == 0.0f) ? 0 : (dot2 / dot);
	if (delta < 0.0f)
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
	if (intersect_ray(PointA, RayDirection,
					  TriangleA, TriangleB, TriangleC,
					  EdgeAB, EdgeBC, EdgeCA, TriangleNormal,
					  IntersectPoint, distance))
	{
		DirectX::XMStoreFloat3(&nearlest_point_segment, IntersectPoint);
		DirectX::XMStoreFloat3(&nearlest_point_triangle, IntersectPoint);
		return 0.0f;
	}
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

	collision::line ab = collision::line(collision::line::types::segment, a, b);
	float distance_edge = line.calculate_distance(horizon1, delta1, horizon2, delta2, ab);
	if (distance_edge < nearlest_distance)
	{
		nearlest_distance = distance_edge;
		DirectX::XMStoreFloat3(&nearlest_point_segment, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon1), delta1));
		DirectX::XMStoreFloat3(&nearlest_point_triangle, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon2), delta2));
	}

	collision::line bc = collision::line(collision::line::types::segment, b, c);
	distance_edge = line.calculate_distance(horizon1, delta1, horizon2, delta2, bc);
	if (distance_edge < nearlest_distance)
	{
		nearlest_distance = distance_edge;
		DirectX::XMStoreFloat3(&nearlest_point_segment, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon1), delta1));
		DirectX::XMStoreFloat3(&nearlest_point_triangle, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&horizon2), delta2));
	}

	collision::line ca = collision::line(collision::line::types::segment, c, a);
	distance_edge = line.calculate_distance(horizon1, delta1, horizon2, delta2, ca);
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

	DirectX::XMMATRIX transform_matrix = calculate_transform_matrix(mesh.node_index) * data.WorldMatrix;

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
			DirectX::XMStoreFloat3(&positions.emplace_back(), DirectX::XMVector3TransformCoord(P, transform_matrix));
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
		}
	}

	out_mesh = collision::bounding_mesh(positions, triangles, data.separate_level);
	return true;
}
