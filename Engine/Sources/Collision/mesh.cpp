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

#include <thread>
#include <algorithm>

namespace
{
	inline bool plane_aabb(const math::float3& n, const float d, const math::float3& c, const math::float3& h)
	{
		float e = h.x * fabsf(n.x)
				+ h.y * fabsf(n.y)
				+ h.z * fabsf(n.z);
		float s = (c.x * n.x + c.y * n.y + c.z * n.z) + d;
		if (s - e > 0) return false; // 半空間外(法線の逆側)
		if (s + e < 0) return false; // 半空間内(法線側)
		return true;
	}

	inline bool triangle_aabb(const math::float3& u0,
							  const math::float3& u1,
							  const math::float3& u2,
							  const math::float3& n,
							  const collision::inner::axis_aligned_bounding_box& box)
	{
		// 三角形の頂点をAABB中心を原点とした座標系に移動
		math::float3 v[3];
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
		math::float3 bmin, bmax;
		bmin.x = -box.get_local_scale().x;
		bmin.y = -box.get_local_scale().y;
		bmin.z = -box.get_local_scale().z;
		bmax = box.get_local_scale();
		
		math::float3 tmin = v[0], tmax = v[0];
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
		math::float3 e[3] = { {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} };
		math::float3 f[3] = {};
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
				math::float3 a, p;
				math::vector3f A = math::vector3f::cross(math::vector3f(e[i]), math::vector3f(f[j]));
				a = A.to_float3();
				p.x = math::vector3f::dot(A, math::vector3f(v[0]));
				p.y = math::vector3f::dot(A, math::vector3f(v[1]));
				p.z = math::vector3f::dot(A, math::vector3f(v[2]));
				float r = box.get_local_scale().x * fabs(a.x)
						+ box.get_local_scale().y * fabs(a.y)
						+ box.get_local_scale().z * fabs(a.z);
				if(std::min(p.x, std::min(p.y, p.z)) > r || std::max(p.x, std::max(p.y, p.z)) < -r)
					return false;
			}
		}

		return true;
	}

	inline UINT calculate_array_index_from_point(const math::float3& point, const math::float3& area_min, const math::float3& area_unit, math::uint3 separate_count)
	{
		auto cell = [](float value, float amin, float unit, UINT count) -> int
		{
			int i = static_cast<int>(floorf((value - amin) / unit));
			return std::clamp(i, 0, static_cast<int>(count) - 1);
		};
		const UINT ix = static_cast<UINT>(cell(point.x, area_min.x, area_unit.x, separate_count.x));
		const UINT iy = static_cast<UINT>(cell(point.y, area_min.y, area_unit.y, separate_count.y));
		const UINT iz = static_cast<UINT>(cell(point.z, area_min.z, area_unit.z, separate_count.z));
		return ix + iy * separate_count.x + iz * separate_count.x * separate_count.y;
	}

	std::vector<UINT> calculate_check_order_list(const math::float3& min, const math::float3& max, const math::float3& area_min, const math::float3& area_unit, math::uint3 separate_count)
	{
		auto cell = [](float value, float amin, float unit, UINT count) -> int
		{
			int i = static_cast<int>(floorf((value - amin) / unit));
			return std::clamp(i, 0, static_cast<int>(count) - 1);
		};

		const float grid_max_x = area_min.x + area_unit.x * static_cast<float>(separate_count.x);
		const float grid_max_y = area_min.y + area_unit.y * static_cast<float>(separate_count.y);
		const float grid_max_z = area_min.z + area_unit.z * static_cast<float>(separate_count.z);
		if (max.x < area_min.x || min.x > grid_max_x ||
			max.y < area_min.y || min.y > grid_max_y ||
			max.z < area_min.z || min.z > grid_max_z)
			return {};

		const int x0 = cell(min.x, area_min.x, area_unit.x, separate_count.x);
		const int x1 = cell(max.x, area_min.x, area_unit.x, separate_count.x);
		const int y0 = cell(min.y, area_min.y, area_unit.y, separate_count.y);
		const int y1 = cell(max.y, area_min.y, area_unit.y, separate_count.y);
		const int z0 = cell(min.z, area_min.z, area_unit.z, separate_count.z);
		const int z1 = cell(max.z, area_min.z, area_unit.z, separate_count.z);

		std::vector<UINT> order_list;
		order_list.reserve(static_cast<size_t>(x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1));
		for (int z = z0; z <= z1; ++z)
		{
			for (int y = y0; y <= y1; ++y)
			{
				for (int x = x0; x <= x1; ++x)
				{
					order_list.emplace_back(static_cast<UINT>(x)
						+ static_cast<UINT>(y) * separate_count.x
						+ static_cast<UINT>(z) * separate_count.x * separate_count.y);
				}
			}
		}
		return order_list;
	}


	bool calculate_nearlest_point(math::vector3f P,
											  math::vector3f A, math::vector3f B, math::vector3f C,
											  math::vector3f EdgeAB, math::vector3f EdgeBC, math::vector3f EdgeAC,
											  math::vector3f& NearlestPoint)
	{
		math::vector3f AP = P - A;

		float d1 = math::vector3f::dot(EdgeAB, AP);
		float d2 = math::vector3f::dot(EdgeAC, AP);
		if (d1 <= 0.0f && d2 <= 0.0f)
		{
			NearlestPoint = A;
			return false;
		}

		math::vector3f bp = P - B;
		float d3 = math::vector3f::dot(EdgeAB, bp);
		float d4 = math::vector3f::dot(EdgeAC, bp);
		if (d3 >= 0.0f && d4 <= d3)
		{
			NearlestPoint = B;
			return false;
		}

		math::vector3f cp = P - C;
		float d5 = math::vector3f::dot(EdgeAB, cp);
		float d6 = math::vector3f::dot(EdgeAC, cp);
		if (d6 >= 0.0f && d5 <= d6)
		{
			NearlestPoint = C;
			return false;
		}

		float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
		{
			NearlestPoint = (EdgeAB * (d1 / (d1 - d3))) + A;
			return false;
		}

		float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
		{
			NearlestPoint = (EdgeAC * (d2 / (d2 - d6))) + A;
			return false;
		}

		float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		{
			float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			NearlestPoint = (EdgeBC * w) + B;
			return false;
		}

		//	最近点が三角形内の点と判断
		float denom = 1.0f / (va + vb + vc);
		math::vector3f ab = EdgeAB * (vb * denom);
		math::vector3f ac = EdgeAC * (vc * denom);
		NearlestPoint = (ab + ac) + A;
		return true;
	}

	bool intersect_ray(math::vector3f Start, math::vector3f Direction,
								   math::vector3f A, math::vector3f B, math::vector3f C,
								   math::vector3f EdgeAB, math::vector3f EdgeBC, math::vector3f EdgeCA,
								   math::vector3f Normal,
								   math::vector3f& InsertPoint, float& distance, float max_delta = 1.0f)
	{
		float dot = math::vector3f::dot(Direction, Normal);
		if (dot >= 0.0f)
			return	false;

		float dot2 = math::vector3f::dot(A - Start, Normal);
		float delta = (dot2 == 0.0f) ? 0 : (dot2 / dot);
		if (delta < 0.0f || delta > max_delta)
			return	false;

		math::vector3f IP = (Direction * delta) + Start;

		math::vector3f Work;
		Work = math::vector3f::cross(EdgeAB, A - IP);
		if (math::vector3f::dot(Work, Normal) < 0.0f)
			return false;

		Work = math::vector3f::cross(EdgeBC, B - IP);
		if (math::vector3f::dot(Work, Normal) < 0.0f)
			return false;

		Work = math::vector3f::cross(EdgeCA, C - IP);
		if (math::vector3f::dot(Work, Normal) < 0.0f)
			return false;

		InsertPoint = IP;
		distance = delta;
		return	true;
	}

	bool intersect_ray_moller(math::vector3f Start, math::vector3f Direction,
										  math::vector3f A, math::vector3f B, math::vector3f C,
										  math::vector3f EdgeAB, math::vector3f EdgeCA,
										  math::vector3f& InsertPoint, float& distance, float max_delta = 1.0f)
	{
		math::vector3f e1 = EdgeAB;
		math::vector3f e2 = -EdgeCA;

		math::vector3f alpha = math::vector3f::cross(Direction, e2);
		float det = math::vector3f::dot(e1, alpha);
		if (det <= 1e-6f)
			return false;

		float inv_det = 1.0f / det;
		math::vector3f r = Start - A;

		float u = math::vector3f::dot(alpha, r) * inv_det;
		if (u < 0.0f || u > 1.0f)
			return false;

		math::vector3f beta = math::vector3f::cross(r, e1);

		float v = math::vector3f::dot(Direction, beta) * inv_det;
		if (v < 0.0f || u + v > 1.0f)
			return false;

		float t = math::vector3f::dot(e2, beta) * inv_det;
		if (t < 0.0f || t > max_delta)
			return false;

		InsertPoint = (Direction * t) + Start;
		distance = t;
		return	true;
	}


	bool ray_intersects_triangle_both_side(math::vector3f Start, math::vector3f Direction,
		math::vector3f A, math::vector3f EdgeAB, math::vector3f EdgeCA)
	{
		constexpr float epsilon = 1e-6f;

		math::vector3f e2 = -EdgeCA;	//	e2 = C - A

		math::vector3f pvec = math::vector3f::cross(Direction, e2);
		float det = math::vector3f::dot(EdgeAB, pvec);
		if (fabsf(det) < epsilon)
			return false;	//	レイと三角形が平行

		float inv_det = 1.0f / det;
		math::vector3f tvec = Start - A;
		float u = math::vector3f::dot(tvec, pvec) * inv_det;
		if (u < 0.0f || u > 1.0f)
			return false;

		math::vector3f qvec = math::vector3f::cross(tvec, EdgeAB);
		float v = math::vector3f::dot(Direction, qvec) * inv_det;
		if (v < 0.0f || u + v > 1.0f)
			return false;

		float t = math::vector3f::dot(e2, qvec) * inv_det;
		return t > epsilon;	//	始点より前方でのみ交差とみなす
	}

	constexpr math::float3 inside_test_ray_directions[] =
	{
		{ 0.5773503f,  0.5773503f,  0.5773503f},
		{ 0.7071068f, -0.7071068f,  0.4082483f},
		{-0.4082483f,  0.5773503f, -0.7071068f},
	};}

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

const math::float3& bounding_mesh::get_local_scale() const
{
	static const math::float3 scale = {1.0f, 1.0f, 1.0f};
	return	scale;
}

void bounding_mesh::set_local_scale(const math::float3& scale)
{
	//	何もしない
}

bounding_mesh::bounding_mesh()
: bounding_object({ 1, 1, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0 })
{
}

bounding_mesh::bounding_mesh(std::vector<math::float3>& positions, std::vector<triangle>& triangles, UINT separate_level)
	: bounding_object({1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0})
	, positions(positions)
	, triangles(triangles)
	, separate_count(1, 1, 1)
{
	//	頂点が無い場合は空のまま
	if (positions.empty())
	{
		area_min = area_max = area_size = area_unit = math::float3(0.0f, 0.0f, 0.0f);
		return;
	}

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
		if (separate_count.x < 1) separate_count.x = 1;
		if (separate_count.y < 1) separate_count.y = 1;
		if (separate_count.z < 1) separate_count.z = 1;
		area_unit.x = area_size.x / static_cast<float>(separate_count.x);
		area_unit.y = area_size.y / static_cast<float>(separate_count.y);
		area_unit.z = area_size.z / static_cast<float>(separate_count.z);
	}

	//	ブロック本体を先に全部構築（AABBだけ。三角形リストは後で埋める）
	block_datas.resize(static_cast<size_t>(separate_count.x) * separate_count.y * separate_count.z);

	//	三角形の事前計算
	struct triangle_cache
	{
		math::float3 a, b, c;
		math::vector3f triangle_min, triangle_max;
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
			auto A = math::vector3f(tc.a);
			auto B = math::vector3f(tc.b);
			auto C = math::vector3f(tc.c);
			tc.triangle_min = (math::min)((math::min)(A, B), C);
			tc.triangle_max = (math::max)((math::max)(A, B), C);
		}
	}

	{
		math::float3 extent = area_unit;
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

			math::float3 tmin, tmax;
			tmin = math::vector3f(tc.triangle_min).to_float3();
			tmax = math::vector3f(tc.triangle_max).to_float3();

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

						math::vector3f BoxMin, BoxMax;
						data.bounds.include_corner(BoxMin, BoxMax);

						if (math::any_greater(tc.triangle_min, BoxMax) || math::any_greater(BoxMin, tc.triangle_max))
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
		math::float3 extent;
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
			auto A = math::vector3f(*(p  + triangle.a));
			auto B = math::vector3f(*(p  + triangle.b));
			auto C = math::vector3f(*(p  + triangle.c));
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

bool bounding_mesh::intersect_point_inner(const math::float3& point, float& nearlest, const std::vector<size_t>& triangle_indices) const
{
	if (triangle_indices.empty())
		return false;

	math::vector3f P(point);
	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	nearlest = FLT_MAX;
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];

		math::vector3f A(*(p + triangle->a));
		math::vector3f B(*(p + triangle->b));
		math::vector3f C(*(p + triangle->c));
		math::vector3f AB(triangle->ab);
		math::vector3f BC(triangle->bc);
		math::vector3f CA(triangle->ca);
		math::vector3f AC = -CA;

		math::vector3f NP;
		calculate_nearlest_point(P, A, B, C, AB, BC, AC, NP);
		float t = (NP - P).square_length();
		if (t < nearlest)
			nearlest = t;
	}
	return nearlest < FLT_EPSILON;
}

bool bounding_mesh::intersect_point(const math::float3& point) const
{
	math::float3 local_point;
	local_point = (math::vector3f::transform_coord(math::vector3f(point), get_inverse_transform_matrix())).to_float3();

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

bool bounding_mesh::intersect_segment_inner(const math::float3& start, const math::float3& end, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	auto Start = math::vector3f(start);
	auto End = math::vector3f(end);
	auto Vec = End - Start;
	auto Length = Vec.length();
	auto Dir = Vec / Length;

	auto neart = Length;

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	const size_t triangle_count = triangle_indices.size();

	bool hit = false;
	math::vector3f HitPosition, HitNormal, Cross;
	for (size_t index = 0; index < triangle_count; ++index)
	{
		auto triangle = t[ti[index]];
		math::vector3f Normal(triangle.normal);
		float Dot = math::vector3f::dot(Normal, Dir);
		if (Dot >= 0)
			continue;

		math::vector3f A(*(p + triangle.a));

		math::vector3f V = A - Start;
		float T = (math::vector3f::dot(Normal, V)) / Dot;

		float t = T;
		if (t < 0 || t > neart)
			continue;

		math::vector3f IP = Dir * T + Start;

		V = A - IP;
		Cross = math::vector3f::cross(A - IP, math::vector3f(triangle.ab));
		Dot = math::vector3f::dot(Cross, Normal);
		if (Dot < 0)
			continue;

		math::vector3f B(*(p + triangle.b));
		Cross = math::vector3f::cross(B - IP, math::vector3f(triangle.bc));
		Dot = math::vector3f::dot(Cross, Normal);
		if (Dot < 0)
			continue;

		math::vector3f C(*(p + triangle.c));
		Cross = math::vector3f::cross(C - IP, math::vector3f(triangle.ca));
		Dot = math::vector3f::dot(Cross, Normal);
		if (Dot < 0)
			continue;

		neart = t;
		HitPosition = IP;
		HitNormal = Normal;
		hit = true;
	}
	if (hit && intersect)
	{
		intersect_data::infomation infomation;
		infomation.point = HitPosition.to_float3();
		infomation.normal = HitNormal.to_float3();
		infomation.length = neart;
		intersect->add(infomation);
	}
	return	hit;
}

bool bounding_mesh::intersect_segment_inner_moller(const math::float3& start, const math::float3& end, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	auto Start = math::vector3f(start);
	auto End = math::vector3f(end);
	auto Vec = End - Start;
	auto Length = Vec.length();
	auto Dir = Vec / Length;
	auto neart = Length;

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	const size_t triangle_count = triangle_indices.size();

	bool hit = false;
	math::float3 hit_normal;
	math::vector3f HitPosition;
	for (size_t index = 0; index < triangle_count; ++index)
	{
		auto triangle = t[ti[index]];

		math::vector3f A(*(p + triangle.a));
		math::vector3f e1(triangle.ab);				//	e1 = B - A
		math::vector3f e2 = -math::vector3f(triangle.ca);	//	e2 = C - A (= -CA)

		math::vector3f alpha = math::vector3f::cross(Dir, e2);
		float det = math::vector3f::dot(e1, alpha);

		if (det <= 1e-6f)
			continue;

		float inv_det = 1.0f / det;
		math::vector3f r = Start - A;

		float u = math::vector3f::dot(alpha, r) * inv_det;
		if (u < 0.0f || u > 1.0f)
			continue;

		math::vector3f beta = math::vector3f::cross(r, e1);

		float v = math::vector3f::dot(Dir, beta) * inv_det;
		if (v < 0.0f || u + v > 1.0f)
			continue;

		float tt = math::vector3f::dot(e2, beta) * inv_det;
		if (tt < 0.0f || tt > neart)
			continue;

		neart = tt;
		HitPosition = Dir * (math::vector3f(tt, tt, tt)) + Start;
		hit_normal = triangle.normal;
		hit = true;
	}

	if (hit && intersect)
	{
		intersect_data::infomation infomation;
		infomation.point = HitPosition.to_float3();
		infomation.normal = hit_normal;
		infomation.length = neart;
		intersect->add(infomation);
	}
	return	hit;
}

bool bounding_mesh::intersect_segment(const math::float3& start, const math::float3& end, intersect_data* intersect) const
{
	math::float3 local_start, local_end;
	local_start = (math::vector3f::transform_coord(math::vector3f(start), get_inverse_transform_matrix())).to_float3();
	local_end = (math::vector3f::transform_coord(math::vector3f(end), get_inverse_transform_matrix())).to_float3();

	math::float3 min = local_start, max = local_start;
	min.x = std::min(min.x, local_end.x);
	min.y = std::min(min.y, local_end.y);
	min.z = std::min(min.z, local_end.z);
	max.x = std::max(max.x, local_end.x);
	max.y = std::max(max.y, local_end.y);
	max.z = std::max(max.z, local_end.z);
	if ((math::vector3f(local_end) - math::vector3f(local_start)).square_length() <= 1e-12f)
		return false;

	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
	if (list.empty())
		return false;

	bool hit = false;
	intersect_data data, best;
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
				//	最も近い交点を採用
				if (best.size() == 0 || data[0].length < best[0].length)
					best = data;
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
		size_t begin = intersect->size();
		intersect->append(best);
		intersect->multiply_matrices(begin, get_transform_matrix());
	}
	return hit;
}

bool bounding_mesh::intersect_sphere_inner(const math::float3& point, float radius, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	math::vector3f P(point);
	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	math::vector3f NealestPoint = {};
	math::vector3f NealestNormal(0.0f, 1.0f, 0.0f);
	float nearlest = FLT_MAX;
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];
		math::vector3f A(*(p + triangle->a));
		math::vector3f B(*(p + triangle->b));
		math::vector3f C(*(p + triangle->c));
		math::vector3f AB(triangle->ab);
		math::vector3f BC(triangle->bc);
		math::vector3f CA(triangle->ca);
		math::vector3f AC = -CA;

		math::vector3f NP;
		calculate_nearlest_point(P, A, B, C, AB, BC, AC, NP);
		float t = (NP - P).square_length();
		if (t < nearlest)
		{
			nearlest = t;
			NealestPoint = NP;
			NealestNormal = math::vector3f(triangle->normal);
		}
	}
	nearlest = sqrtf(nearlest);
	nearlest -= radius;
	if (nearlest <= 0.0f && intersect)
	{
		P = math::vector3f(point);
		math::vector3f Vector = NealestPoint - P;
		//	中心が面上なら面法線の逆向き
		if (Vector.square_length() <= 1e-12f)
			Vector = -NealestNormal;
		Vector = Vector.normalized();
		math::float3 p, n;
		p = math::vector3f(NealestPoint).to_float3();
		n = Vector.to_float3();
		intersect->add(p, n, fabsf(nearlest));
	}
	return nearlest <= 0.0f;
}

bool bounding_mesh::intersect_sphere(const math::float3& point, float radius, intersect_data* intersect) const
{
	math::float3 local_point;
	local_point = (math::vector3f::transform_coord(math::vector3f(point), get_inverse_transform_matrix())).to_float3();

	math::float3 min, max;
	min.x = local_point.x - radius;
	min.y = local_point.y - radius;
	min.z = local_point.z - radius;
	max.x = local_point.x + radius;
	max.y = local_point.y + radius;
	max.z = local_point.z + radius;
	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);

	bool hit = false;
	intersect_data data, best;
	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (!intersect)
		{
			if (intersect_sphere_inner(local_point, radius, node.triangles, nullptr))
				return true;
			continue;
		}

		//	全ブロックから最も深い接触を採用
		data.clear();
		if (intersect_sphere_inner(local_point, radius, node.triangles, &data))
		{
			if (best.size() == 0 || data[0].length > best[0].length)
				best = data;
			hit = true;
		}
	}

	if (hit && intersect)
	{
		//	ワールドへ変換
		size_t begin = intersect->size();
		intersect->append(best);
		intersect->multiply_matrices(begin, get_transform_matrix());
	}
	return hit;
}

bool bounding_mesh::intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect) const
{
	return	intersect_sphere(sphere.get_position(), sphere.get_radius(), intersect);
}

bool bounding_mesh::intersect_capsule_inner(const bounding_capsule& local_capsule, math::vector3f CapsuleMin, math::vector3f CapsuleMax, math::matrixf TransformMatrix, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	auto local_capsule_line = local_capsule.get_line();
	auto radius = local_capsule.get_radius();
	auto LineA = math::vector3f(local_capsule_line[0]);
	auto LineB = math::vector3f(local_capsule_line[1]);

	math::float3 nearlest_point_segment = {}, nearlest_point_triangle = {};
	bool nearlest_point_edghFlg = false;
	float nearlest = FLT_MAX;
	size_t nealest_face_index = -1;
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];

		//	ボックス判定
		auto Min = math::vector3f(triangle->min);
		auto Max = math::vector3f(triangle->max);
		if (math::any_greater(CapsuleMin, Max) || math::any_greater(Min, CapsuleMax))
			continue;

		//	平面距離判定
		if (nearlest < FLT_MAX)
		{
			auto TriA = math::vector3f(*(p + triangle->a));
			auto Normal = math::vector3f(triangle->normal);

			float dA = math::vector3f::dot(Normal, LineA - TriA);
			float dB = math::vector3f::dot(Normal, LineB - TriA);

			if (dA * dB > 0.0f)
			{
				if (std::min(fabsf(dA), fabsf(dB)) >= nearlest)
					continue;
			}
		}

		//	距離算出
		math::float3 near_point_segment, near_point_triangle;
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
		auto triangle = t + ti[nealest_face_index];
		math::vector3f TriangleA(*(p + triangle->a));
		math::vector3f TriangleB(*(p + triangle->b));
		math::vector3f TriangleC(*(p + triangle->c));
		math::vector3f PointA(nearlest_point_segment);
		math::vector3f PointB(nearlest_point_triangle);

		math::vector3f Position(local_capsule.get_line()[0]);
		math::vector3f End(local_capsule.get_line()[1]);
		math::vector3f Direction = (Position - End).normalized();

		collision::intersect_data::infomation infomation;
		infomation.point = nearlest_point_segment;
		infomation.point2 = nearlest_point_triangle;
		math::vector3f Vector = PointA - PointB;

		//	最短点が面上の場合
		math::vector3f Normal(triangle->normal);

		if (!nearlest_point_edghFlg)
		{
			//	線分が三角形を貫通している場合
			if (nearlest == 0)
			{
				//	両端の表裏判定
				float d = math::vector3f::dot(Normal, Direction);
				math::vector3f BackPoint = d > 0 ? End : Position;

				//	裏側の点から交点へのベクトルを算出し、カプセルのポジションに加算
				//	さらに法線の単位ベクトルにカプセルの半径を掛けたベクトルを用意し、カプセルのポジションに加算する
				Vector = (PointB - BackPoint) + (Normal * radius);

				infomation.length = Vector.length();
				Normal = Vector.normalized();
			}
			else
			{
				infomation.length = radius - Vector.length();
				Normal = Vector.normalized();
			}
		}
		else
		{
			//	最短点が辺上の場合
			infomation.length = radius - nearlest;
		//	infomation.length = radius - DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
			Normal = Vector.normalized();
		}

		//	法線が手に入らなかった場合は三角形の面を納入
		if (math::vector3f::dot(Normal, Normal) <= FLT_EPSILON)
		{
			Normal = math::vector3f(triangle->normal);
		}

		//	法線を反転しておく
		infomation.normal = (-Normal).to_float3();

		infomation.point = (math::vector3f::transform_coord(math::vector3f(infomation.point), TransformMatrix)).to_float3();
		infomation.point2 = (math::vector3f::transform_coord(math::vector3f(infomation.point2), TransformMatrix)).to_float3();
		infomation.normal = ((math::vector3f::transform_normal(math::vector3f(infomation.normal), TransformMatrix)).normalized()).to_float3();
		intersect->add(infomation);
	}
	return nearlest < radius;
}

bool bounding_mesh::intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect) const
{
	//	ローカル空間に変換
	bounding_capsule local_capsule = capsule;
	{
		math::matrixf CopyTransformMatrix = local_capsule.get_transform_matrix();
		math::vector3f Scale, Translation;
		math::quaternionf Rotation;
		(CopyTransformMatrix * get_inverse_transform_matrix()).decompose(Scale, Rotation, Translation);
		math::float4 rotation;
		math::float3 translation;
		rotation = Rotation.to_float4();
		translation = Translation.to_float3();
		local_capsule.set_rotation(rotation);
		local_capsule.set_position(translation);
	}

	//	ローカル空間のAABB
	math::vector3f Min, Max;
	math::float3 min, max;
	local_capsule.include_corner(min, max);
	Min = math::vector3f(min);
	Max = math::vector3f(max);

	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
	if (list.empty())
		return false;

	bool hit = false;
	intersect_data data, best;
	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (node.triangles.empty())
			continue;

		if (!node.bounds.intersect_capsule(local_capsule))
			continue;

		if (intersect)
		{
			data.clear();
			if (intersect_capsule_inner(local_capsule, Min, Max, get_transform_matrix(), node.triangles, &data))
			{
				//	最も深い接触を採用
				if (best.size() == 0 || data[0].length > best[0].length)
					best = data;
				hit = true;
			}
		}
		else
		{
			if (intersect_capsule_inner(local_capsule, Min, Max, get_transform_matrix(), node.triangles, intersect))
				return true;
		}
	}

	if (hit && intersect)
		intersect->append(best);
	return hit;
}

bool bounding_mesh::intersect_box_inner(math::vector3f Center, math::vector3f AxisX, math::vector3f AxisY, math::vector3f AxisZ,
										 const math::float3& half_extent, math::vector3f BoxMin, math::vector3f BoxMax,
										 const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();

	inner::axis_aligned_bounding_box local_box(half_extent, math::float3{0.0f, 0.0f, 0.0f});

	auto to_box_space = [&](math::vector3f V)
	{
		math::vector3f D = V - Center;
		math::float3 result;
		result.x = math::vector3f::dot(D, AxisX);
		result.y = math::vector3f::dot(D, AxisY);
		result.z = math::vector3f::dot(D, AxisZ);
		return result;
	};

	bool hit = false;
	float nearlest = FLT_MAX;
	math::vector3f NearlestPoint = {};
	math::vector3f NearlestNormal = AxisY;
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];

		math::vector3f TriMin(triangle->min);
		math::vector3f TriMax(triangle->max);
		if (math::any_greater(TriMin, BoxMax) || math::any_greater(BoxMin, TriMax))
			continue;

		math::vector3f A(*(p + triangle->a));
		math::vector3f B(*(p + triangle->b));
		math::vector3f C(*(p + triangle->c));
		math::vector3f Normal(triangle->normal);

		math::float3 la = to_box_space(A);
		math::float3 lb = to_box_space(B);
		math::float3 lc = to_box_space(C);
		math::float3 ln;
		ln.x = math::vector3f::dot(Normal, AxisX);
		ln.y = math::vector3f::dot(Normal, AxisY);
		ln.z = math::vector3f::dot(Normal, AxisZ);

		if (!triangle_aabb(la, lb, lc, ln, local_box))
			continue;

		hit = true;

		if (!intersect)
			continue;

		math::vector3f AB(triangle->ab);
		math::vector3f BC(triangle->bc);
		math::vector3f CA(triangle->ca);
		math::vector3f AC = -CA;

		math::vector3f NP;
		calculate_nearlest_point(Center, A, B, C, AB, BC, AC, NP);

		float distance_square = (NP - Center).square_length();
		if (distance_square < nearlest)
		{
			nearlest = distance_square;
			NearlestPoint = NP;
			NearlestNormal = Normal;
		}
	}

	if (hit && intersect)
	{
		math::vector3f Vector = NearlestPoint - Center;
		float distance = sqrtf(nearlest);
		//	中心が面上なら面法線の逆向き
		math::vector3f Direction = (distance > FLT_EPSILON) ? Vector.normalized() : -NearlestNormal;

		float support = fabsf(math::vector3f::dot(AxisX, Direction)) * half_extent.x
					   + fabsf(math::vector3f::dot(AxisY, Direction)) * half_extent.y
					   + fabsf(math::vector3f::dot(AxisZ, Direction)) * half_extent.z;

		math::float3 point, normal;
		point = math::vector3f(NearlestPoint).to_float3();
		normal = Direction.to_float3();
		intersect->add(point, normal, std::max(support - distance, 0.0f));
	}

	return hit;
}

bool bounding_mesh::intersect_box(const bounding_box& box, intersect_data* intersect) const
{
	math::vector3f Scale, Translation;
	math::quaternionf Rotation;
	(box.get_transform_matrix() * get_inverse_transform_matrix()).decompose(Scale, Rotation, Translation);

	math::matrixf RotationMatrix = math::matrixf::rotation_quaternion(Rotation);
	math::vector3f AxisX = RotationMatrix.to_vector3(0).normalized();
	math::vector3f AxisY = RotationMatrix.to_vector3(1).normalized();
	math::vector3f AxisZ = RotationMatrix.to_vector3(2).normalized();
	math::vector3f Center = Translation;

	const math::float3& half_extent = box.get_local_scale();

	math::vector3f Extent = ((math::abs(AxisX) * half_extent.x) + (math::abs(AxisY) * half_extent.y)) + (math::abs(AxisZ) * half_extent.z);
	math::vector3f BoxMin = Center - Extent;
	math::vector3f BoxMax = Center + Extent;

	math::float3 min, max;
	min = BoxMin.to_float3();
	max = BoxMax.to_float3();

	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
	if (list.empty())
		return false;

	//	ローカル空間の箱
	math::float4 local_rotation;
	math::float3 local_position;
	local_rotation = Rotation.to_float4();
	local_position = Translation.to_float3();
	bounding_box local_box(half_extent, local_rotation, local_position);

	bool hit = false;
	intersect_data data, best;
	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (node.triangles.empty())
			continue;

		if (!node.bounds.intersect_box(local_box))
			continue;

		if (intersect)
		{
			data.clear();
			if (intersect_box_inner(Center, AxisX, AxisY, AxisZ, half_extent, BoxMin, BoxMax, node.triangles, &data))
			{
				//	最も深い接触を採用
				if (best.size() == 0 || data[0].length > best[0].length)
					best = data;
				hit = true;
			}
		}
		else
		{
			if (intersect_box_inner(Center, AxisX, AxisY, AxisZ, half_extent, BoxMin, BoxMax, node.triangles, nullptr))
				return true;
		}
	}

	if (hit && intersect)
	{
		//	ワールドへ変換
		size_t begin = intersect->size();
		intersect->append(best);
		intersect->multiply_matrices(begin, get_transform_matrix());
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

	math::vector3f Start(sweep_line[0]);
	math::vector3f End(sweep_line[1]);
	math::vector3f SweepDirectionNormalize = (End - Start).normalized();
	math::vector3f qp = Start - End;

	//	球のAABB
	math::vector3f RadiusVec(radius, radius, radius);
	math::vector3f CastMin = ((math::min)(Start, End)) - RadiusVec;
	math::vector3f CastMax = ((math::max)(Start, End)) + RadiusVec;

	bool hit_flag = false;
	float nearlest_distance = FLT_MAX;
	math::vector3f nearlest_triangle_pos = {};
	math::vector3f nearlest_normal = {};
	for (size_t index = 0; index < triangle_indices.size(); ++index)
	{
		auto triangle = t + ti[index];

		math::vector3f Min(triangle->min);
		math::vector3f Max(triangle->max);
		if (math::any_greater(CastMin, Max) || math::any_greater(Min, CastMax))
			continue;

		math::vector3f TriangleA(*(p + triangle->a));
		math::vector3f TriangleB(*(p + triangle->b));
		math::vector3f TriangleC(*(p + triangle->c));
		math::vector3f EdgeAB(triangle->ab);
		math::vector3f EdgeBC(triangle->bc);
		math::vector3f EdgeCA(triangle->ca);
		math::vector3f EdgeAC = -EdgeCA;

		//	開始位置ですでに球と三角形が交差している場合は、この三角形での判定を行わない
		math::vector3f NearestOnTriangle;
		calculate_nearlest_point(Start, TriangleA, TriangleB, TriangleC, EdgeAB, EdgeBC, EdgeAC, NearestOnTriangle);
		if ((NearestOnTriangle - Start).square_length() <= radius * radius)
		{
			//	表側から面へ向かう場合のみ移動距離0で接触
			math::vector3f FaceNormal(triangle->normal);
			if (math::vector3f::dot(Start - TriangleA, FaceNormal) < 0.0f)
				continue;

			math::vector3f Vector = Start - NearestOnTriangle;
			math::vector3f OverlapNormal = (Vector.square_length() > 1e-12f) ? Vector.normalized() : FaceNormal;
			if (math::vector3f::dot(SweepDirectionNormalize, OverlapNormal) >= 0.0f)
				continue;

			if (0.0f < nearlest_distance)
			{
				nearlest_distance = 0.0f;
				nearlest_triangle_pos = NearestOnTriangle;
				nearlest_normal = OverlapNormal;
				hit_flag = true;
			}
			continue;
		}

		//	三角形の法線（正規化しない：後続の平面判定のスケールに用いるため）
		math::vector3f Norm = math::vector3f::cross(EdgeAB, EdgeAC);
		float d = math::vector3f::dot(Norm, qp);

		bool triangle_hit = false;
		float hit_distance = 0.0f;
		math::vector3f hit_position = {};

		if (d >= 0.0f)	//	表側から接近している場合のみ判定する
		{
			//	面領域の判定
			math::vector3f fix_vec = Norm.normalized() * radius;
			math::vector3f ap = Start - (TriangleA + fix_vec);
			float plane_t = math::vector3f::dot(Norm, ap);
			if (plane_t >= 0.0f && plane_t < d)
			{
				math::vector3f e = math::vector3f::cross(qp, ap);
				float v = math::vector3f::dot(EdgeAC, e);
				if (v >= 0.0f && v <= d)
				{
					float w = -math::vector3f::dot(EdgeAB, e);
					if (w > 0.0f && v + w <= d)
					{
						math::vector3f cross_pos = Start + (qp * (-plane_t / d));
						hit_distance = (cross_pos - Start).length();
						hit_position = cross_pos - fix_vec;
						triangle_hit = true;
					}
				}
			}
		}

		if (!triangle_hit)
		{
			//	面領域で交差しなかった場合は、頂点・辺のボロノイ領域で最短距離の交点を求める
			float bound_distance = qp.length();

			const math::vector3f verts[3] = { TriangleA, TriangleB, TriangleC };
			for (int v = 0; v < 3; ++v)
			{
				math::vector3f ray2sphere = verts[v] - Start;
				float projection = math::vector3f::dot(ray2sphere, SweepDirectionNormalize);
				if (projection <= 0.0f)
					continue;

				float dist_sq = ray2sphere.square_length() - projection * projection;
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
				math::vector3f p0, p1;
			}
			const edges[3] =
			{
				{ TriangleA, TriangleB },
				{ TriangleA, TriangleC },
				{ TriangleB, TriangleC },
			};

			for (int e = 0; e < 3; ++e)
			{
				math::vector3f cyl_pos;
				float cyl_dist;
				if (intersect_ray_vs_cylinder(Start, End, edges[e].p0, edges[e].p1, radius, cyl_pos, cyl_dist))
				{
					if (bound_distance > cyl_dist)
					{
						math::vector3f edge_dir = (edges[e].p1 - edges[e].p0).normalized();
						math::vector3f vec0p = cyl_pos - edges[e].p0;
						bound_distance = cyl_dist;
						hit_distance = cyl_dist;
						hit_position = (math::vector3f::dot(vec0p, edge_dir)) * edge_dir + edges[e].p0;
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
			nearlest_normal = Norm.normalized();
			hit_flag = true;
		}
	}

	if (intersect && hit_flag)
	{
		//	接触時点の球中心位置
		math::vector3f SphereCenterAtHit = Start + (SweepDirectionNormalize * nearlest_distance);

		collision::intersect_data::infomation infomation;
		infomation.point = SphereCenterAtHit.to_float3();		//	接触時点の球の中心位置
		infomation.point2 = math::vector3f(nearlest_triangle_pos).to_float3();	//	三角形側の接触点
		infomation.length = nearlest_distance;								//	接触までの移動距離
		infomation.normal = math::vector3f(nearlest_normal).to_float3();		//	接触面の法線
		intersect->add(infomation);
	}
	return hit_flag;
}

bool bounding_mesh::intersect_sphere_cast(const math::float3& start, const math::float3& end, float radius, intersect_data* intersect) const
{
	math::float3 local_start, local_end;
	local_start = (math::vector3f::transform_coord(math::vector3f(start), get_inverse_transform_matrix())).to_float3();
	local_end = (math::vector3f::transform_coord(math::vector3f(end), get_inverse_transform_matrix())).to_float3();

	math::float3 min, max;
	min.x = std::min(local_start.x, local_end.x) - radius;
	min.y = std::min(local_start.y, local_end.y) - radius;
	min.z = std::min(local_start.z, local_end.z) - radius;
	max.x = std::max(local_start.x, local_end.x) + radius;
	max.y = std::max(local_start.y, local_end.y) + radius;
	max.z = std::max(local_start.z, local_end.z) + radius;

	auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
	if (list.empty())
		return false;

	if ((math::vector3f(local_end) - math::vector3f(local_start)).square_length() <= 1e-12f)
		return false;

	collision::line sweep_line(collision::line::types::segment, local_start, local_end);

	bool hit = false;
	intersect_data data, best;
	for (auto node_index : list)
	{
		auto& node = block_datas[node_index];
		if (node.triangles.empty())
			continue;

		if (intersect)
		{
			data.clear();
			if (intersect_sphere_cast_inner(sweep_line, radius, node.triangles, &data))
			{
				//	最も近い交点を採用
				if (best.size() == 0 || data[0].length < best[0].length)
					best = data;
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
		size_t begin = intersect->size();
		intersect->append(best);
		intersect->multiply_matrices(begin, get_transform_matrix());
	}

	return hit;
}

bool bounding_mesh::intersect_capsule_cast(const math::float3& start, const math::float3& end,
										   float radius, float length, const math::float4& rotation,
										   intersect_data* intersect) const
{
	math::vector3f axis = math::quaternionf(rotation).rotate(math::vector3f(0.0f, 1.0f, 0.0f));
	return intersect_capsule_cast_by_axis_spheres(start, end, radius, length, axis, intersect);
}

void bounding_mesh::include_corner(math::float3& min, math::float3& max) const
{
	//	ローカル範囲の8頂点をワールドへ変換して包含
	math::matrixf Transform = get_transform_matrix();
	math::vector3f Min(FLT_MAX, FLT_MAX, FLT_MAX);
	math::vector3f Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for (int corner = 0; corner < 8; ++corner)
	{
		math::float3 point(
			(corner & 1) ? area_max.x : area_min.x,
			(corner & 2) ? area_max.y : area_min.y,
			(corner & 4) ? area_max.z : area_min.z);
		math::vector3f Point = math::vector3f::transform_coord(math::vector3f(point), Transform);
		Min = (math::min)(Min, Point);
		Max = (math::max)(Max, Point);
	}
	min = Min.to_float3();
	max = Max.to_float3();
}

float bounding_mesh::calculate_segment_distance(const collision::line& line,
												const triangle& triangle,
												math::float3& nearlest_point_segment,
												math::float3& nearlest_point_triangle,
												bool& nearlest_point_edghFlg) const
{
	nearlest_point_edghFlg = false;

	//	線分と三角形の交差判定
	auto p = positions.data();
	math::vector3f PointA(line[0]);
	math::vector3f PointB(line[1]);
	math::vector3f TriangleA(*(p + triangle.a));
	math::vector3f TriangleB(*(p + triangle.b));
	math::vector3f TriangleC(*(p + triangle.c));
	math::vector3f EdgeAB(triangle.ab);
	math::vector3f EdgeBC(triangle.bc);
	math::vector3f EdgeCA(triangle.ca);
	math::vector3f EdgeAC = -EdgeCA;
	math::vector3f TriangleNormal(triangle.normal);

	math::vector3f RayDirection = PointB - PointA;
	math::vector3f IntersectPoint;
	float distance;
	if (intersect_ray_moller(PointA, RayDirection,
					  TriangleA, TriangleB, TriangleC,
					  EdgeAB, EdgeCA,
					  IntersectPoint, distance))
	{
		nearlest_point_segment = IntersectPoint.to_float3();
		nearlest_point_triangle = IntersectPoint.to_float3();
		return 0.0f;
	}
#if 0	//	逆方向からの二回目は破棄。復活させる場合は delta > max_delta 判定が不要
	RayDirection = -RayDirection;
	if (intersect_ray(PointB, RayDirection,
					  TriangleA, TriangleB, TriangleC,
					  EdgeAB, EdgeBC, EdgeCA, TriangleNormal,
					  IntersectPoint, distance))
	{
		nearlest_point_segment = IntersectPoint.to_float3();
		nearlest_point_triangle = IntersectPoint.to_float3();
		return 0.0f;
	}
#endif

	//	線分の各頂点に対して三角形の最近点を求める
	math::vector3f NearPosA, NearPosB;
	bool insideA = calculate_nearlest_point(PointA, TriangleA, TriangleB, TriangleC,
											EdgeAB, EdgeBC, EdgeAC, NearPosA);
	bool insideB = calculate_nearlest_point(PointB, TriangleA, TriangleB, TriangleC,
											EdgeAB, EdgeBC, EdgeAC, NearPosB);

	float nearlest_distance = FLT_MAX;
	if (insideA || insideB)
	{
		float distance_squareA, distance_squareB;
		distance_squareA = (NearPosA - PointA).square_length();
		distance_squareB = (NearPosB - PointB).square_length();

		if (distance_squareA < distance_squareB)
		{
			nearlest_distance = sqrtf(distance_squareA);
			nearlest_point_segment = PointA.to_float3();
			nearlest_point_triangle = NearPosA.to_float3();
		}
		else
		{
			nearlest_distance = sqrtf(distance_squareB);
			nearlest_point_segment = PointB.to_float3();
			nearlest_point_triangle = NearPosB.to_float3();
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
	math::float3 a = *(p + triangle.a);
	math::float3 b = *(p + triangle.b);
	math::float3 c = *(p + triangle.c);

	//	三角形外部に線分（または端）が存在し、最近点が辺上になる場合、線分と三角形の３辺の距離を全て計測し比較する
	math::float3 horizon1, horizon2;
	float delta1, delta2;

	float distance_edge = line.calculate_distance(horizon1, delta1, horizon2, delta2, triangle.lab);
	if (distance_edge < nearlest_distance)
	{
		nearlest_distance = distance_edge;
		nearlest_point_segment = horizon1;
		nearlest_point_triangle = horizon2;
	}

	distance_edge = line.calculate_distance(horizon1, delta1, horizon2, delta2, triangle.lbc);
	if (distance_edge < nearlest_distance)
	{
		nearlest_distance = distance_edge;
		nearlest_point_segment = horizon1;
		nearlest_point_triangle = horizon2;
	}

	distance_edge = line.calculate_distance(horizon1, delta1, horizon2, delta2, triangle.lca);
	if (distance_edge < nearlest_distance)
	{
		nearlest_distance = distance_edge;
		nearlest_point_segment = horizon1;
		nearlest_point_triangle = horizon2;
	}

	return nearlest_distance;
}

bool bounding_mesh::is_point_inside(const math::float3& point) const
{
	if (triangles.empty() || block_datas.empty())
		return false;

	math::float3 local_point;
	local_point = (math::vector3f::transform_coord(math::vector3f(point), get_inverse_transform_matrix())).to_float3();
	math::vector3f P(local_point);

	constexpr size_t direction_count = std::size(inside_test_ray_directions);
	math::vector3f directions[direction_count];
	UINT hit_counts[direction_count] = {};
	for (size_t index = 0; index < direction_count; ++index)
		directions[index] = math::vector3f(inside_test_ray_directions[index]);

	math::float3 grid_extent = { area_max.x - area_min.x, area_max.y - area_min.y, area_max.z - area_min.z };
	float ray_length = sqrtf(grid_extent.x * grid_extent.x + grid_extent.y * grid_extent.y + grid_extent.z * grid_extent.z)
		+ std::max({ area_unit.x, area_unit.y, area_unit.z });

	auto p = positions.data();
	std::vector<size_t> candidate_triangles;
	for (size_t index = 0; index < direction_count; ++index)
	{
		math::float3 local_end;
		local_end = (P + (directions[index] * ray_length)).to_float3();

		math::float3 min = local_point, max = local_point;
		min.x = std::min(min.x, local_end.x); min.y = std::min(min.y, local_end.y); min.z = std::min(min.z, local_end.z);
		max.x = std::max(max.x, local_end.x); max.y = std::max(max.y, local_end.y); max.z = std::max(max.z, local_end.z);

		candidate_triangles.clear();
		auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
		for (auto node_index : list)
		{
			auto& node = block_datas[node_index];
			if (node.triangles.empty())
				continue;
			if (!node.bounds.intersect_segment(local_point, local_end))
				continue;

			candidate_triangles.insert(candidate_triangles.end(), node.triangles.begin(), node.triangles.end());
		}
		std::sort(candidate_triangles.begin(), candidate_triangles.end());
		candidate_triangles.erase(std::unique(candidate_triangles.begin(), candidate_triangles.end()), candidate_triangles.end());

		for (auto triangle_index : candidate_triangles)
		{
			auto& triangle = triangles[triangle_index];
			math::vector3f A(*(p + triangle.a));
			math::vector3f AB(triangle.ab);
			math::vector3f CA(triangle.ca);

			if (ray_intersects_triangle_both_side(P, directions[index], A, AB, CA))
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

bool bounding_mesh::distance_to_point(const math::float3& point, float& out_distance, math::float3* out_nearest_point, math::float3* out_normal) const
{
	if (triangles.empty() || block_datas.empty())
		return false;

	math::float3 local_point;
	local_point = (math::vector3f::transform_coord(math::vector3f(point), get_inverse_transform_matrix())).to_float3();
	math::vector3f P(local_point);

	auto p = positions.data();
	auto t = triangles.data();

	std::vector<bool> checked(block_datas.size(), false);

	float nearest_sq = FLT_MAX;
	math::vector3f nearest_point_v = P;
	math::float3 nearest_normal = { 0.0f, 1.0f, 0.0f };

	float search_radius = std::max({ area_unit.x, area_unit.y, area_unit.z });
	if (search_radius <= 0.0f)
		search_radius = 1.0f;

	for (int safety = 0; safety < 32; ++safety)
	{
		math::float3 min = { local_point.x - search_radius, local_point.y - search_radius, local_point.z - search_radius };
		math::float3 max = { local_point.x + search_radius, local_point.y + search_radius, local_point.z + search_radius };
		auto list = calculate_check_order_list(min, max, area_min, area_unit, separate_count);
		bool covers_whole_grid = list.size() >= block_datas.size();

		for (auto node_index : list)
		{
			if (checked[node_index])
				continue;
			checked[node_index] = true;

			auto& node = block_datas[node_index];
			for (auto triangle_index : node.triangles)
			{
				auto& triangle = t[triangle_index];
				math::vector3f A(*(p + triangle.a));
				math::vector3f B(*(p + triangle.b));
				math::vector3f C(*(p + triangle.c));
				math::vector3f AB(triangle.ab);
				math::vector3f BC(triangle.bc);
				math::vector3f CA(triangle.ca);
				math::vector3f AC = -CA;

				math::vector3f NP;
				calculate_nearlest_point(P, A, B, C, AB, BC, AC, NP);
				float d = (NP - P).square_length();
				if (d < nearest_sq)
				{
					nearest_sq = d;
					nearest_point_v = NP;
					nearest_normal = triangle.normal;
				}
			}
		}

		if (nearest_sq < FLT_MAX && sqrtf(nearest_sq) <= search_radius)
			break;
		if (covers_whole_grid)
			break;

		search_radius *= 2.0f;
	}

	if (nearest_sq >= FLT_MAX)
		return false;

	out_distance = sqrtf(nearest_sq);
	if (out_nearest_point)
	{
		*out_nearest_point = (math::vector3f::transform_coord(nearest_point_v, get_transform_matrix())).to_float3();
	}
	if (out_normal)
	{
		*out_normal = ((math::vector3f::transform_normal(math::vector3f(nearest_normal), get_transform_matrix())).normalized()).to_float3();
	}
	return true;
}

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
