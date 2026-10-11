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
#include <fstream>
#include <functional>

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
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
		const math::float3 box_position(box.get_position().to_float3());
		const math::float3 box_scale(box.get_local_scale());

		math::float3 v[3];
		v[0].x = u0.x - box_position.x;
		v[0].y = u0.y - box_position.y;
		v[0].z = u0.z - box_position.z;
		v[1].x = u1.x - box_position.x;
		v[1].y = u1.y - box_position.y;
		v[1].z = u1.z - box_position.z;
		v[2].x = u2.x - box_position.x;
		v[2].y = u2.y - box_position.y;
		v[2].z = u2.z - box_position.z;

		// 1. 三角形のAABBとAABBの交差判定
		math::float3 bmin, bmax;
		bmin.x = -box_scale.x;
		bmin.y = -box_scale.y;
		bmin.z = -box_scale.z;
		bmax = box_scale;
		
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
		if(!plane_aabb(n, -(n.x * v[0].x + n.y * v[0].y + n.z * v[0].z), {0.0f, 0.0f, 0.0f}, box_scale))
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
				float r = box_scale.x * fabs(a.x)
						+ box_scale.y * fabs(a.y)
						+ box_scale.z * fabs(a.z);
				if(std::min(p.x, std::min(p.y, p.z)) > r || std::max(p.x, std::max(p.y, p.z)) < -r)
					return false;
			}
		}

		return true;
	}

	//	@tool BVH構築パラメータ
	constexpr std::uint32_t bvh_max_leaf_triangles = 4;		//	@tool 葉に入れる三角形の目安
	constexpr std::uint32_t bvh_force_leaf_triangles = 16;	//	@tool 分割しても得をしない場合に葉で許す上限
	constexpr int bvh_max_depth = 48;						//	@tool 構築の最大深度（走査スタックの上限を兼ねる）
	constexpr int bvh_bin_count = 16;						//	@tool SAH評価の分割候補数
	constexpr int bvh_stack_size = bvh_max_depth * 2 + 2;

	inline bool aabb_overlap(const math::float3& min1, const math::float3& max1, const math::float3& min2, const math::float3& max2)
	{
		return min1.x <= max2.x && max1.x >= min2.x
			&& min1.y <= max2.y && max1.y >= min2.y
			&& min1.z <= max2.z && max1.z >= min2.z;
	}

	inline float aabb_surface_area(const math::float3& min, const math::float3& max)
	{
		const float x = max.x - min.x;
		const float y = max.y - min.y;
		const float z = max.z - min.z;
		return 2.0f * (x * y + y * z + z * x);
	}

	inline void aabb_include(math::float3& min, math::float3& max, const math::float3& point_min, const math::float3& point_max)
	{
		min.x = std::min(min.x, point_min.x); min.y = std::min(min.y, point_min.y); min.z = std::min(min.z, point_min.z);
		max.x = std::max(max.x, point_max.x); max.y = std::max(max.y, point_max.y); max.z = std::max(max.z, point_max.z);
	}

	//	@tool 線分(start→end)が、margin だけ膨らませたAABBと交わるか（スラブ法）
	inline bool segment_overlap_aabb(const math::float3& start, const math::float3& end, const math::float3& min, const math::float3& max, float margin)
	{
		const float s[3] = { start.x, start.y, start.z };
		const float d[3] = { end.x - start.x, end.y - start.y, end.z - start.z };
		const float bmin[3] = { min.x - margin, min.y - margin, min.z - margin };
		const float bmax[3] = { max.x + margin, max.y + margin, max.z + margin };

		float t_min = 0.0f;
		float t_max = 1.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (fabsf(d[axis]) < 1e-12f)
			{
				if (s[axis] < bmin[axis] || s[axis] > bmax[axis])
					return false;
				continue;
			}

			const float inv = 1.0f / d[axis];
			float t1 = (bmin[axis] - s[axis]) * inv;
			float t2 = (bmax[axis] - s[axis]) * inv;
			if (t1 > t2)
				std::swap(t1, t2);
			t_min = std::max(t_min, t1);
			t_max = std::min(t_max, t2);
			if (t_min > t_max)
				return false;
		}
		return true;
	}

	//	@tool 点とAABBの最短距離の2乗
	inline float point_aabb_distance_square(const math::float3& point, const math::float3& min, const math::float3& max)
	{
		const float dx = std::max({ min.x - point.x, 0.0f, point.x - max.x });
		const float dy = std::max({ min.y - point.y, 0.0f, point.y - max.y });
		const float dz = std::max({ min.z - point.z, 0.0f, point.z - max.z });
		return dx * dx + dy * dy + dz * dz;
	}

	//	@tool BVHを走査する（node_test が false のノードは子孫ごと飛ばす。leaf が true を返したら打ち切り）
	template<typename NodeTest, typename LeafFunction>
	bool traverse_bvh(const std::vector<collision::bounding_mesh::bvh_node>& nodes, NodeTest&& node_test, LeafFunction&& leaf)
	{
		if (nodes.empty())
			return false;

		std::uint32_t stack[bvh_stack_size];
		int stack_count = 0;
		stack[stack_count++] = 0;
		while (stack_count > 0)
		{
			const std::uint32_t node_index = stack[--stack_count];
			const auto& node = nodes[node_index];
			if (!node_test(node))
				continue;

			if (node.is_leaf())
			{
				if (leaf(node))
					return true;
				continue;
			}

			stack[stack_count++] = node.offset;		//	右の子
			stack[stack_count++] = node_index + 1;	//	左の子
		}
		return false;
	}

	//	@tool 指定範囲と重なる葉から、範囲と重なる三角形番号だけを集める（BVHでは三角形の重複は無い）
	template<typename NodeTest>
	void gather_bvh_triangles(const std::vector<collision::bounding_mesh::bvh_node>& nodes,
							  const std::vector<std::uint32_t>& triangle_indices,
							  const std::vector<collision::bounding_mesh::triangle>& triangles,
							  const math::float3& query_min, const math::float3& query_max,
							  NodeTest&& node_test, std::vector<size_t>& out)
	{
		out.clear();
		traverse_bvh(nodes,
			[&](const collision::bounding_mesh::bvh_node& node)
			{
				return aabb_overlap(node.min, node.max, query_min, query_max) && node_test(node);
			},
			[&](const collision::bounding_mesh::bvh_node& node)
			{
				for (std::uint32_t index = node.offset; index < node.offset + node.count; ++index)
				{
					const std::uint32_t triangle_index = triangle_indices[index];
					const auto& triangle = triangles[triangle_index];
					if (aabb_overlap(triangle.min.to_float3(), triangle.max.to_float3(), query_min, query_max))
						out.push_back(triangle_index);
				}
				return false;
			});
	}

	inline void gather_bvh_triangles(const std::vector<collision::bounding_mesh::bvh_node>& nodes,
									 const std::vector<std::uint32_t>& triangle_indices,
									 const std::vector<collision::bounding_mesh::triangle>& triangles,
									 const math::float3& query_min, const math::float3& query_max,
									 std::vector<size_t>& out)
	{
		gather_bvh_triangles(nodes, triangle_indices, triangles, query_min, query_max,
							 [](const collision::bounding_mesh::bvh_node&) { return true; }, out);
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

namespace
{
	//	@tool 旧形式（均一グリッド）のブロック情報。旧ファイルの読み飛ばし専用
	struct legacy_block_data
	{
		math::float3 center;
		std::vector<size_t> triangles;

		template<class Archive>
		void serialize(Archive& archive, int version)
		{
			archive(CEREAL_NVP(center), CEREAL_NVP(triangles));
		}
	};

	//	@tool BVH形式のファイル先頭に置く識別値（旧形式の先頭は頂点数なので重ならない）
	constexpr std::uint64_t bvh_format_magic = 0x3148564245424D45ull;

	inline float get_axis(const math::float3& value, int axis)
	{
		return axis == 0 ? value.x : (axis == 1 ? value.y : value.z);
	}
}

// CEREALバージョン定義
CEREAL_CLASS_VERSION(bounding_mesh::triangle, 1)
CEREAL_CLASS_VERSION(bounding_mesh::bvh_node, 1)
CEREAL_CLASS_VERSION(legacy_block_data, 1)

template<class Archive>
void bounding_mesh::triangle::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(a), CEREAL_NVP(b), CEREAL_NVP(c));
}

template<class Archive>
void bounding_mesh::bvh_node::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(min), CEREAL_NVP(max), CEREAL_NVP(offset), CEREAL_NVP(count));
}

const math::double3& bounding_mesh::get_local_scale() const
{
	static const math::double3 scale = {1.0, 1.0, 1.0};
	return	scale;
}

void bounding_mesh::set_local_scale(const math::double3& scale)
{
	//	何もしない
}

bounding_mesh::bounding_mesh()
: bounding_object({ 1, 1, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0 })
{
}

bounding_mesh::bounding_mesh(std::vector<math::vector3f>& positions, std::vector<triangle>& triangles, std::uint32_t separate_level)
	: bounding_object({1, 1, 1}, {0, 0, 0, 1}, {0, 0, 0})
	, positions(positions)
	, triangles(triangles)
{
	(void)separate_level;

	//	頂点が無い場合は空のまま
	if (positions.empty())
	{
		area_min = area_max = math::float3(0.0f, 0.0f, 0.0f);
		return;
	}

	//	幅算出
	area_min = area_max = positions[0].to_float3();
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

	//	@tool BVHは三角形の範囲を使うので、渡された三角形の事前計算に頼らず計算し直す
	precompute_triangles();
	build_bvh();
}

bounding_mesh::~bounding_mesh()
{
}

void bounding_mesh::precompute_triangles()
{
	//	事前計算
	auto p = positions.data();
	for (auto& triangle : triangles)
	{
		const auto& A = *(p  + triangle.a);
		const auto& B = *(p  + triangle.b);
		const auto& C = *(p  + triangle.c);
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

void bounding_mesh::build_bvh()
{
	bvh_nodes.clear();
	bvh_triangle_indices.clear();
	if (triangles.empty())
		return;

	const std::uint32_t triangle_count = static_cast<std::uint32_t>(triangles.size());

	//	@tool 三角形毎の範囲と中心を控えておく
	std::vector<math::float3> triangle_mins(triangle_count);
	std::vector<math::float3> triangle_maxs(triangle_count);
	std::vector<math::float3> triangle_centers(triangle_count);
	bvh_triangle_indices.resize(triangle_count);
	for (std::uint32_t index = 0; index < triangle_count; ++index)
	{
		triangle_mins[index] = triangles[index].min.to_float3();
		triangle_maxs[index] = triangles[index].max.to_float3();
		triangle_centers[index] =
		{
			(triangle_mins[index].x + triangle_maxs[index].x) * 0.5f,
			(triangle_mins[index].y + triangle_maxs[index].y) * 0.5f,
			(triangle_mins[index].z + triangle_maxs[index].z) * 0.5f,
		};
		bvh_triangle_indices[index] = index;
	}
	bvh_nodes.reserve(static_cast<size_t>(triangle_count) * 2 / bvh_max_leaf_triangles + 1);

	struct bin
	{
		math::float3 min = { FLT_MAX, FLT_MAX, FLT_MAX };
		math::float3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		std::uint32_t count = 0;
	};

	std::function<std::uint32_t(std::uint32_t, std::uint32_t, int)> build_node =
		[&](std::uint32_t begin, std::uint32_t end, int depth) -> std::uint32_t
	{
		const std::uint32_t node_index = static_cast<std::uint32_t>(bvh_nodes.size());
		bvh_nodes.emplace_back();

		//	@tool ノードの範囲と、三角形中心の範囲を求める
		math::float3 bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
		math::float3 bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		math::float3 center_min = bounds_min;
		math::float3 center_max = bounds_max;
		for (std::uint32_t index = begin; index < end; ++index)
		{
			const std::uint32_t triangle_index = bvh_triangle_indices[index];
			aabb_include(bounds_min, bounds_max, triangle_mins[triangle_index], triangle_maxs[triangle_index]);
			aabb_include(center_min, center_max, triangle_centers[triangle_index], triangle_centers[triangle_index]);
		}
		bvh_nodes[node_index].min = bounds_min;
		bvh_nodes[node_index].max = bounds_max;

		const std::uint32_t count = end - begin;
		auto make_leaf = [&]()
		{
			bvh_nodes[node_index].offset = begin;
			bvh_nodes[node_index].count = count;
			return node_index;
		};
		if (count <= bvh_max_leaf_triangles || depth >= bvh_max_depth)
			return make_leaf();

		//	@tool 三角形中心の広がりが最大の軸で分割する
		int axis = 0;
		{
			const float extent_x = center_max.x - center_min.x;
			const float extent_y = center_max.y - center_min.y;
			const float extent_z = center_max.z - center_min.z;
			if (extent_y > extent_x && extent_y >= extent_z) axis = 1;
			else if (extent_z > extent_x && extent_z > extent_y) axis = 2;
		}
		const float axis_min = get_axis(center_min, axis);
		const float axis_extent = get_axis(center_max, axis) - axis_min;

		std::uint32_t middle = begin;
		if (axis_extent > 1e-12f)
		{
			//	@tool ビン分割でSAHコストが最小になる位置を探す
			bin bins[bvh_bin_count];
			const float scale = static_cast<float>(bvh_bin_count) / axis_extent;
			auto calculate_bin = [&](std::uint32_t triangle_index)
			{
				const int bin_index = static_cast<int>((get_axis(triangle_centers[triangle_index], axis) - axis_min) * scale);
				return std::clamp(bin_index, 0, bvh_bin_count - 1);
			};
			for (std::uint32_t index = begin; index < end; ++index)
			{
				const std::uint32_t triangle_index = bvh_triangle_indices[index];
				bin& target = bins[calculate_bin(triangle_index)];
				aabb_include(target.min, target.max, triangle_mins[triangle_index], triangle_maxs[triangle_index]);
				++target.count;
			}

			float left_areas[bvh_bin_count - 1];
			std::uint32_t left_counts[bvh_bin_count - 1];
			{
				bin accumulate;
				for (int split = 0; split < bvh_bin_count - 1; ++split)
				{
					if (bins[split].count > 0)
						aabb_include(accumulate.min, accumulate.max, bins[split].min, bins[split].max);
					accumulate.count += bins[split].count;
					left_counts[split] = accumulate.count;
					left_areas[split] = accumulate.count > 0 ? aabb_surface_area(accumulate.min, accumulate.max) : 0.0f;
				}
			}

			float best_cost = FLT_MAX;
			int best_split = -1;
			{
				bin accumulate;
				for (int split = bvh_bin_count - 2; split >= 0; --split)
				{
					if (bins[split + 1].count > 0)
						aabb_include(accumulate.min, accumulate.max, bins[split + 1].min, bins[split + 1].max);
					accumulate.count += bins[split + 1].count;
					if (left_counts[split] == 0 || accumulate.count == 0)
						continue;

					const float cost = left_areas[split] * static_cast<float>(left_counts[split])
									 + aabb_surface_area(accumulate.min, accumulate.max) * static_cast<float>(accumulate.count);
					if (cost < best_cost)
					{
						best_cost = cost;
						best_split = split;
					}
				}
			}

			//	@tool 分割しても得をしない少数の三角形はそのまま葉にする
			const float leaf_cost = aabb_surface_area(bounds_min, bounds_max) * static_cast<float>(count);
			if (best_split < 0 || (best_cost >= leaf_cost && count <= bvh_force_leaf_triangles))
			{
				if (count <= bvh_force_leaf_triangles)
					return make_leaf();
			}
			else
			{
				auto it = std::partition(bvh_triangle_indices.begin() + begin, bvh_triangle_indices.begin() + end,
										 [&](std::uint32_t triangle_index) { return calculate_bin(triangle_index) <= best_split; });
				middle = static_cast<std::uint32_t>(it - bvh_triangle_indices.begin());
			}
		}

		//	@tool 分割できなかった場合は中心位置の中央値で半分にする
		if (middle == begin || middle == end)
		{
			if (count <= bvh_force_leaf_triangles)
				return make_leaf();

			middle = begin + count / 2;
			std::nth_element(bvh_triangle_indices.begin() + begin, bvh_triangle_indices.begin() + middle, bvh_triangle_indices.begin() + end,
							 [&](std::uint32_t lhs, std::uint32_t rhs) { return get_axis(triangle_centers[lhs], axis) < get_axis(triangle_centers[rhs], axis); });
		}

		build_node(begin, middle, depth + 1);
		const std::uint32_t right_index = build_node(middle, end, depth + 1);
		bvh_nodes[node_index].offset = right_index;
		bvh_nodes[node_index].count = 0;
		return node_index;
	};
	build_node(0, triangle_count, 0);
}

bool bounding_mesh::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			std::uint64_t format_magic = bvh_format_magic;
			archive(CEREAL_NVP(format_magic),
					CEREAL_NVP(positions),
					CEREAL_NVP(triangles),

					CEREAL_NVP(area_min),
					CEREAL_NVP(area_max),
					CEREAL_NVP(bvh_nodes),
					CEREAL_NVP(bvh_triangle_indices));
		}
		catch (...)
		{
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
		//	@tool 先頭の識別値で、BVH形式か旧形式（均一グリッド）かを判別する
		std::uint64_t head = 0;
		istream.read(reinterpret_cast<char*>(&head), sizeof(head));
		istream.clear();
		istream.seekg(0);
		const bool is_bvh_format = head == bvh_format_magic;

		cereal::BinaryInputArchive archive(istream);

		try
		{
			if (is_bvh_format)
			{
				std::uint64_t format_magic = 0;
				archive(CEREAL_NVP(format_magic),
						CEREAL_NVP(positions),
						CEREAL_NVP(triangles),

						CEREAL_NVP(area_min),
						CEREAL_NVP(area_max),
						CEREAL_NVP(bvh_nodes),
						CEREAL_NVP(bvh_triangle_indices));
			}
			else
			{
				//	@tool 旧形式は三角形まで使い、グリッド情報は読み捨ててBVHを作り直す
				math::float3 area_size;
				math::float3 area_unit;
				math::uint3 separate_count;
				std::vector<legacy_block_data> block_datas;
				archive(CEREAL_NVP(positions),
						CEREAL_NVP(triangles),

						CEREAL_NVP(area_min),
						CEREAL_NVP(area_max),
						CEREAL_NVP(area_size),
						CEREAL_NVP(area_unit),
						CEREAL_NVP(separate_count),
						CEREAL_NVP(block_datas));
			}
		}
		catch (...)
		{
			assert(!"bounding_mesh deserialize failed.\n");
			return false;
		}

		precompute_triangles();
		if (!is_bvh_format)
			build_bvh();
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

bool bounding_mesh::intersect_point(const math::double3& point) const
{
	math::float3 local_point;
	local_point = (math::vector3d::transform_coord(math::vector3d(point), get_inverse_transform_matrix())).to_float3();

	float nearlest = FLT_MAX;

	//	@tool 点を含む葉の三角形だけを判定する（面上判定の誤差分だけ範囲を広げる）
	constexpr float point_margin = 1e-3f;
	const math::float3 min = { local_point.x - point_margin, local_point.y - point_margin, local_point.z - point_margin };
	const math::float3 max = { local_point.x + point_margin, local_point.y + point_margin, local_point.z + point_margin };

	thread_local std::vector<size_t> candidates;
	gather_bvh_triangles(bvh_nodes, bvh_triangle_indices, triangles, min, max, candidates);
	return intersect_point_inner(local_point, nearlest, candidates);
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
		infomation.point = math::double3(HitPosition.to_float3());
		infomation.normal = math::double3(HitNormal.to_float3());
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
		hit_normal = triangle.normal.to_float3();
		hit = true;
	}

	if (hit && intersect)
	{
		intersect_data::infomation infomation;
		infomation.point = math::double3(HitPosition.to_float3());
		infomation.normal = math::double3(hit_normal);
		infomation.length = neart;
		intersect->add(infomation);
	}
	return	hit;
}

bool bounding_mesh::intersect_segment(const math::double3& start, const math::double3& end, intersect_data* intersect) const
{
	math::float3 local_start, local_end;
	local_start = (math::vector3d::transform_coord(math::vector3d(start), get_inverse_transform_matrix())).to_float3();
	local_end = (math::vector3d::transform_coord(math::vector3d(end), get_inverse_transform_matrix())).to_float3();

	math::float3 min = local_start, max = local_start;
	min.x = std::min(min.x, local_end.x);
	min.y = std::min(min.y, local_end.y);
	min.z = std::min(min.z, local_end.z);
	max.x = std::max(max.x, local_end.x);
	max.y = std::max(max.y, local_end.y);
	max.z = std::max(max.z, local_end.z);
	if ((math::vector3f(local_end) - math::vector3f(local_start)).square_length() <= 1e-12f)
		return false;


	//	@tool 線分が通る葉の三角形を集め、まとめて最も近い交点を求める
	thread_local std::vector<size_t> candidates;
	gather_bvh_triangles(bvh_nodes, bvh_triangle_indices, triangles, min, max,
		[&](const bvh_node& node) { return segment_overlap_aabb(local_start, local_end, node.min, node.max, 0.0f); },
		candidates);

	if (!intersect)
		return intersect_segment_inner_moller(local_start, local_end, candidates);

	intersect_data best;
	if (!intersect_segment_inner_moller(local_start, local_end, candidates, &best))
		return false;

	//	ローカル空間からワールド空間へ変換
	size_t begin = intersect->size();
	intersect->append(best);
	intersect->multiply_matrices(begin, get_transform_matrix());
	return true;
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

bool bounding_mesh::intersect_sphere(const math::double3& point, double world_radius, intersect_data* intersect) const
{
	const float radius = static_cast<float>(world_radius);
	math::float3 local_point;
	local_point = (math::vector3d::transform_coord(math::vector3d(point), get_inverse_transform_matrix())).to_float3();

	math::float3 min, max;
	min.x = local_point.x - radius;
	min.y = local_point.y - radius;
	min.z = local_point.z - radius;
	max.x = local_point.x + radius;
	max.y = local_point.y + radius;
	max.z = local_point.z + radius;

	//	@tool 球の範囲と重なる葉の三角形を集め、まとめて最も深い接触を求める
	thread_local std::vector<size_t> candidates;
	gather_bvh_triangles(bvh_nodes, bvh_triangle_indices, triangles, min, max, candidates);

	if (!intersect)
		return intersect_sphere_inner(local_point, radius, candidates, nullptr);

	intersect_data best;
	if (!intersect_sphere_inner(local_point, radius, candidates, &best))
		return false;

	//	ワールドへ変換
	size_t begin = intersect->size();
	intersect->append(best);
	intersect->multiply_matrices(begin, get_transform_matrix());
	return true;
}

bool bounding_mesh::intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect) const
{
	return	intersect_sphere(sphere.get_position().to_double3(), sphere.get_radius(), intersect);
}

bool bounding_mesh::intersect_capsule_inner(const bounding_capsule& local_capsule, math::vector3f CapsuleMin, math::vector3f CapsuleMax, const math::matrixd& TransformMatrix, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
{
	if (triangle_indices.empty())
		return false;

	auto p = positions.data();
	auto ti = triangle_indices.data();
	auto t = triangles.data();
	const auto& local_capsule_double_line = local_capsule.get_line();
	collision::linef local_capsule_line(collision::linef::types::segment, math::float3(local_capsule_double_line[0]), math::float3(local_capsule_double_line[1]));
	const float radius = static_cast<float>(local_capsule.get_radius());
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

		math::vector3f Position(local_capsule_line[0]);
		math::vector3f End(local_capsule_line[1]);
		math::vector3f Direction = (Position - End).normalized();

		collision::intersect_data::infomation infomation;
		infomation.point = math::double3(nearlest_point_segment);
		infomation.point2 = math::double3(nearlest_point_triangle);
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
		infomation.normal = math::double3((-Normal).to_float3());

		infomation.point = (math::vector3d::transform_coord(math::vector3d(infomation.point), TransformMatrix)).to_double3();
		infomation.point2 = (math::vector3d::transform_coord(math::vector3d(infomation.point2), TransformMatrix)).to_double3();
		infomation.normal = ((math::vector3d::transform_normal(math::vector3d(infomation.normal), TransformMatrix)).normalized()).to_double3();
		intersect->add(infomation);
	}
	return nearlest < radius;
}

bool bounding_mesh::intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect) const
{
	//	ローカル空間に変換
	bounding_capsule local_capsule = capsule;
	{
		math::matrixd CopyTransformMatrix = local_capsule.get_transform_matrix();
		math::vector3d Scale, Translation;
		math::quaterniond Rotation;
		(CopyTransformMatrix * get_inverse_transform_matrix()).decompose(Scale, Rotation, Translation);
		math::double4 rotation;
		math::double3 translation;
		rotation = Rotation.to_double4();
		translation = Translation.to_double3();
		local_capsule.set_rotation(rotation);
		local_capsule.set_position(translation);
	}

	//	ローカル空間のAABB
	math::vector3f Min, Max;
	math::float3 min, max;
	{
		math::double3 local_min, local_max;
		local_capsule.include_corner(local_min, local_max);
		min = math::float3(local_min);
		max = math::float3(local_max);
	}
	Min = math::vector3f(min);
	Max = math::vector3f(max);


	//	@tool カプセルの範囲と重なる葉の三角形を集め、まとめて最も深い接触を求める
	thread_local std::vector<size_t> candidates;
	gather_bvh_triangles(bvh_nodes, bvh_triangle_indices, triangles, min, max, candidates);

	if (!intersect)
		return intersect_capsule_inner(local_capsule, Min, Max, get_transform_matrix(), candidates, nullptr);

	intersect_data best;
	if (!intersect_capsule_inner(local_capsule, Min, Max, get_transform_matrix(), candidates, &best))
		return false;

	intersect->append(best);
	return true;
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

	inner::axis_aligned_bounding_box local_box(math::double3(half_extent), math::double3{0.0, 0.0, 0.0});

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
	math::vector3d Scale, Translation;
	math::quaterniond Rotation;
	(box.get_transform_matrix() * get_inverse_transform_matrix()).decompose(Scale, Rotation, Translation);

	math::matrixd RotationMatrix = math::matrixd::rotation_quaternion(Rotation);
	math::vector3f AxisX(RotationMatrix.to_vector3(0).normalized().to_float3());
	math::vector3f AxisY(RotationMatrix.to_vector3(1).normalized().to_float3());
	math::vector3f AxisZ(RotationMatrix.to_vector3(2).normalized().to_float3());
	math::vector3f Center(Translation.to_float3());

	const math::float3 half_extent(box.get_local_scale());

	math::vector3f Extent = ((math::abs(AxisX) * half_extent.x) + (math::abs(AxisY) * half_extent.y)) + (math::abs(AxisZ) * half_extent.z);
	math::vector3f BoxMin = Center - Extent;
	math::vector3f BoxMax = Center + Extent;

	math::float3 min, max;
	min = BoxMin.to_float3();
	max = BoxMax.to_float3();


	//	@tool 箱の範囲と重なる葉の三角形を集め、まとめて最も深い接触を求める
	thread_local std::vector<size_t> candidates;
	gather_bvh_triangles(bvh_nodes, bvh_triangle_indices, triangles, min, max, candidates);

	if (!intersect)
		return intersect_box_inner(Center, AxisX, AxisY, AxisZ, half_extent, BoxMin, BoxMax, candidates, nullptr);

	intersect_data best;
	if (!intersect_box_inner(Center, AxisX, AxisY, AxisZ, half_extent, BoxMin, BoxMax, candidates, &best))
		return false;

	//	ワールドへ変換
	size_t begin = intersect->size();
	intersect->append(best);
	intersect->multiply_matrices(begin, get_transform_matrix());
	return true;
}

bool bounding_mesh::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	//	未対応
//	DEBUG_LOG("not corespound mesh vs mesh\n");
	return false;
}

bool bounding_mesh::intersect_sphere_cast_inner(const collision::linef& sweep_line, float radius, const std::vector<size_t>& triangle_indices, intersect_data* intersect) const
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
				math::vector3d cyl_pos_double;
				double cyl_dist_double;
				if (intersect_ray_vs_cylinder(math::vector3d(Start), math::vector3d(End), math::vector3d(edges[e].p0), math::vector3d(edges[e].p1), radius, cyl_pos_double, cyl_dist_double))
				{
					math::vector3f cyl_pos(cyl_pos_double);
					float cyl_dist = static_cast<float>(cyl_dist_double);
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
		infomation.point = math::double3(SphereCenterAtHit.to_float3());		//	接触時点の球の中心位置
		infomation.point2 = math::double3(math::vector3f(nearlest_triangle_pos).to_float3());	//	三角形側の接触点
		infomation.length = nearlest_distance;								//	接触までの移動距離
		infomation.normal = math::double3(math::vector3f(nearlest_normal).to_float3());		//	接触面の法線
		intersect->add(infomation);
	}
	return hit_flag;
}

bool bounding_mesh::intersect_sphere_cast(const math::double3& start, const math::double3& end, double world_radius, intersect_data* intersect) const
{
	const float radius = static_cast<float>(world_radius);
	math::float3 local_start, local_end;
	local_start = (math::vector3d::transform_coord(math::vector3d(start), get_inverse_transform_matrix())).to_float3();
	local_end = (math::vector3d::transform_coord(math::vector3d(end), get_inverse_transform_matrix())).to_float3();

	math::float3 min, max;
	min.x = std::min(local_start.x, local_end.x) - radius;
	min.y = std::min(local_start.y, local_end.y) - radius;
	min.z = std::min(local_start.z, local_end.z) - radius;
	max.x = std::max(local_start.x, local_end.x) + radius;
	max.y = std::max(local_start.y, local_end.y) + radius;
	max.z = std::max(local_start.z, local_end.z) + radius;


	if ((math::vector3f(local_end) - math::vector3f(local_start)).square_length() <= 1e-12f)
		return false;

	collision::linef sweep_line(collision::linef::types::segment, local_start, local_end);

	//	@tool 移動する球が通り得る葉（線分が半径分膨らませたノードを通る）の三角形を集め、まとめて最も近い交点を求める
	thread_local std::vector<size_t> candidates;
	gather_bvh_triangles(bvh_nodes, bvh_triangle_indices, triangles, min, max,
		[&](const bvh_node& node) { return segment_overlap_aabb(local_start, local_end, node.min, node.max, radius); },
		candidates);

	if (!intersect)
		return intersect_sphere_cast_inner(sweep_line, radius, candidates, nullptr);

	intersect_data best;
	if (!intersect_sphere_cast_inner(sweep_line, radius, candidates, &best))
		return false;

	//	結果をローカル空間からワールド空間へ変換
	size_t begin = intersect->size();
	intersect->append(best);
	intersect->multiply_matrices(begin, get_transform_matrix());
	return true;
}

bool bounding_mesh::intersect_capsule_cast(const math::double3& start, const math::double3& end,
										   double radius, double length, const math::double4& rotation,
										   intersect_data* intersect) const
{
	math::vector3d axis = math::quaterniond(rotation).rotate(math::vector3d(0.0, 1.0, 0.0));
	return intersect_capsule_cast_by_axis_spheres(start, end, radius, length, axis, intersect);
}

void bounding_mesh::include_corner(math::double3& min, math::double3& max) const
{
	//	ローカル範囲の8頂点をワールドへ変換して包含
	math::matrixd Transform = get_transform_matrix();
	math::vector3d Min(DBL_MAX, DBL_MAX, DBL_MAX);
	math::vector3d Max(-DBL_MAX, -DBL_MAX, -DBL_MAX);
	for (int corner = 0; corner < 8; ++corner)
	{
		math::float3 point(
			(corner & 1) ? area_max.x : area_min.x,
			(corner & 2) ? area_max.y : area_min.y,
			(corner & 4) ? area_max.z : area_min.z);
		math::vector3d Point = math::vector3d::transform_coord(math::vector3d(point), Transform);
		Min = (math::min)(Min, Point);
		Max = (math::max)(Max, Point);
	}
	min = Min.to_double3();
	max = Max.to_double3();
}

float bounding_mesh::calculate_segment_distance(const collision::linef& line,
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
	math::float3 a = (p + triangle.a)->to_float3();
	math::float3 b = (p + triangle.b)->to_float3();
	math::float3 c = (p + triangle.c)->to_float3();

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

bool bounding_mesh::is_point_inside(const math::double3& point) const
{
	if (triangles.empty() || bvh_nodes.empty())
		return false;

	math::float3 local_point;
	local_point = (math::vector3d::transform_coord(math::vector3d(point), get_inverse_transform_matrix())).to_float3();
	math::vector3f P(local_point);

	constexpr size_t direction_count = std::size(inside_test_ray_directions);
	math::vector3f directions[direction_count];
	std::uint32_t hit_counts[direction_count] = {};
	for (size_t index = 0; index < direction_count; ++index)
		directions[index] = math::vector3f(inside_test_ray_directions[index]);

	math::float3 grid_extent = { area_max.x - area_min.x, area_max.y - area_min.y, area_max.z - area_min.z };
	float ray_length = sqrtf(grid_extent.x * grid_extent.x + grid_extent.y * grid_extent.y + grid_extent.z * grid_extent.z)
		+ 1.0f;	//	@tool 範囲の外まで確実に伸ばすための余裕（旧: グリッド1マス分）

	auto p = positions.data();
	std::vector<size_t> candidate_triangles;
	for (size_t index = 0; index < direction_count; ++index)
	{
		math::float3 local_end;
		local_end = (P + (directions[index] * ray_length)).to_float3();

		math::float3 min = local_point, max = local_point;
		min.x = std::min(min.x, local_end.x); min.y = std::min(min.y, local_end.y); min.z = std::min(min.z, local_end.z);
		max.x = std::max(max.x, local_end.x); max.y = std::max(max.y, local_end.y); max.z = std::max(max.z, local_end.z);

		//	@tool レイが通る葉の三角形を集める（BVHでは三角形が重複しないので重複除去は不要）
		gather_bvh_triangles(bvh_nodes, bvh_triangle_indices, triangles, min, max,
			[&](const bvh_node& node) { return segment_overlap_aabb(local_point, local_end, node.min, node.max, 0.0f); },
			candidate_triangles);

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

	std::uint32_t inside_votes = 0;
	for (auto hit_count : hit_counts)
	{
		if ((hit_count % 2) == 1)
			++inside_votes;
	}
	return inside_votes * 2 > direction_count;
}

bool bounding_mesh::distance_to_point(const math::double3& point, double& out_distance, math::double3* out_nearest_point, math::double3* out_normal) const
{
	if (triangles.empty() || bvh_nodes.empty())
		return false;

	math::float3 local_point;
	local_point = (math::vector3d::transform_coord(math::vector3d(point), get_inverse_transform_matrix())).to_float3();
	math::vector3f P(local_point);

	auto p = positions.data();
	auto t = triangles.data();

	float nearest_sq = FLT_MAX;
	math::vector3f nearest_point_v = P;
	math::float3 nearest_normal = { 0.0f, 1.0f, 0.0f };

	//	@tool 現在の最短距離より遠いノードは子孫ごと飛ばしながら、最も近い三角形を探す
	{
		traverse_bvh(bvh_nodes,
			[&](const bvh_node& node)
			{
				return point_aabb_distance_square(local_point, node.min, node.max) < nearest_sq;
			},
			[&](const bvh_node& node)
		{
			for (std::uint32_t leaf_index = node.offset; leaf_index < node.offset + node.count; ++leaf_index)
			{
				auto& triangle = t[bvh_triangle_indices[leaf_index]];
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
					nearest_normal = triangle.normal.to_float3();
				}
			}
			return false;
		});
	}

	if (nearest_sq >= FLT_MAX)
		return false;

	out_distance = sqrtf(nearest_sq);
	if (out_nearest_point)
	{
		*out_nearest_point = (math::vector3d::transform_coord(math::vector3d(nearest_point_v.to_float3()), get_transform_matrix())).to_double3();
	}
	if (out_normal)
	{
		*out_normal = ((math::vector3d::transform_normal(math::vector3d(nearest_normal), get_transform_matrix())).normalized()).to_double3();
	}
	return true;
}
