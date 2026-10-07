#include	"collision/box.h"

#include	"collision/sphere.h"
#include	"collision/capsule.h"
#include	"collision/mesh.h"


using namespace collision;

namespace
{
	//	AABBの頂点座標を、u/v（ボロノイ領域を示すビットマスク）から求める
	//	bit 0:x, bit 1:y, bit 2:z　各軸が立っていればmin側、立っていなければmax側の頂点
	static math::vector3d get_aabb_corner(math::vector3d position, math::vector3d extents, int corner_mask)
	{
		math::vector3d Min = position - extents;
		math::vector3d Max = position + extents;
		math::double3 min_pos, max_pos;
		min_pos = Min.to_double3();
		max_pos = Max.to_double3();

		math::double3 corner;
		corner.x = (corner_mask & (1 << 0)) ? min_pos.x : max_pos.x;
		corner.y = (corner_mask & (1 << 1)) ? min_pos.y : max_pos.y;
		corner.z = (corner_mask & (1 << 2)) ? min_pos.z : max_pos.z;
		return math::vector3d(corner);
	}
}

math::matrixd inner::axis_aligned_bounding_box::get_transeform_matrix() const
{
	math::vector3d Scale(get_local_scale());
	math::vector3d Position(get_position());
	return	math::matrixd::scaling(Scale) * math::matrixd::translation(Position);
}

math::double4x4 inner::axis_aligned_bounding_box::get_transeform_float4x4() const
{
	math::double4x4 matrix;
	matrix = get_transeform_matrix().to_double4x4();
	return matrix;
}

inner::axis_aligned_bounding_box::axis_aligned_bounding_box()
: bounding_object()
{
}

inner::axis_aligned_bounding_box::axis_aligned_bounding_box(const math::double3& scale, const math::double3& position)
: bounding_object(scale, math::double4(0, 0, 0, 1), position)
{
}

inner::axis_aligned_bounding_box::~axis_aligned_bounding_box()
{
}

double inner::axis_aligned_bounding_box::calculate_point_to_length(const math::double3& point) const
{
	math::vector3d Position(get_position());
	math::vector3d Scale(get_local_scale());

	math::double3 min, max;
	max = (Position + Scale).to_double3();
	min = (Position - Scale).to_double3();
	math::double3 center = point;

	double* center_ptr = &center.x;
	double* min_ptr = &min.x;
	double* max_ptr = &max.x;
#if 01
	double square_length = 0.0f;
	for (size_t index = 0; index < 3; ++index)
	{
		if (center_ptr[index] < min_ptr[index])
		{
			double l = (center_ptr[index] - min_ptr[index]);
			square_length += l * l;
		}
		if (center_ptr[index] > max_ptr[index])
		{
			double l = (center_ptr[index] - max_ptr[index]);
			square_length += l * l;
		}
	}
	square_length = std::sqrt(square_length);
	return	square_length;
#else
	for (size_t index = 0; index < 3; ++index)
	{
		if (center_ptr[index] < min_ptr[index])
		{
			center_ptr[index] = min_ptr[index];
		}
		if (center_ptr[index] > max_ptr[index])
		{
			center_ptr[index] = max_ptr[index];
		}
	}
	math::vector3d Point(point);
	math::vector3d Center(center);
	return	(Point - Center).length();
#endif
}

bool inner::axis_aligned_bounding_box::intersect_point(const math::double3& point) const
{
	return calculate_point_to_length(point) <= FLT_EPSILON;
}

bool inner::axis_aligned_bounding_box::intersect_segment(const math::double3& start, const math::double3& end, intersect_data* intersect) const
{
	const double* center = &get_position().x;
	const double* extent = &get_local_scale().x;
	const double* s = &start.x;
	const double* e = &end.x;

	//	スラブ法で線分と判定
	double tmin = 0.0f;
	double tmax = 1.0f;
	int hit_axis = -1;
	double hit_sign = 0.0f;
	for (int index = 0; index < 3; ++index)
	{
		double d = e[index] - s[index];
		double min = center[index] - extent[index];
		double max = center[index] + extent[index];
		if (std::fabs(d) <= FLT_EPSILON)
		{
			if (s[index] < min || s[index] > max)
				return false;
			continue;
		}

		double ood = 1.0f / d;
		double t1 = (min - s[index]) * ood;
		double t2 = (max - s[index]) * ood;
		double sign = -1.0f;
		if (t1 > t2)
		{
			std::swap(t1, t2);
			sign = 1.0f;
		}
		if (t1 > tmin)
		{
			tmin = t1;
			hit_axis = index;
			hit_sign = sign;
		}
		if (t2 < tmax)
			tmax = t2;
		if (tmin > tmax)
			return false;
	}

	if (intersect)
	{
		math::vector3d Start(start);
		math::vector3d Vector = math::vector3d(end) - Start;
		math::double3 position, normal(0.0f, 0.0f, 0.0f);
		position = (Vector * (math::vector3d(tmin, tmin, tmin)) + Start).to_double3();
		if (hit_axis >= 0)
			(&normal.x)[hit_axis] = hit_sign;
		else
			normal = (-Vector).to_double3();	//	始点が内側
		intersect->add(position, normal, tmin * Vector.length());
	}
	return true;
}

bool inner::axis_aligned_bounding_box::intersect_sphere(const math::double3& point, double radius, intersect_data* intersect) const
{
	math::vector3d Position(get_position());
	math::vector3d Scale(get_local_scale());

	math::double3 min, max;
	max = (Position + Scale).to_double3();
	min = (Position - Scale).to_double3();

	math::double3 closest_point;
	closest_point.x = math::clamp(point.x, min.x, max.x);
	closest_point.y = math::clamp(point.y, min.y, max.y);
	closest_point.z = math::clamp(point.z, min.z, max.z);

	math::vector3d Point(point);
	math::vector3d ClosestPoint(closest_point);
	math::vector3d VectorC_P = ClosestPoint - Point;
	double length_sq = VectorC_P.square_length();
	if (length_sq >= radius * radius)
		return false;

	if (intersect)
	{
		if (length_sq > 1e-12f)
		{
			//	球から箱へ向かう向きに箱を押し出す
			double length = std::sqrt(length_sq);
			math::double3 normal;
			normal = (VectorC_P * (1.0f / length)).to_double3();
			intersect->add(closest_point, normal, radius - length);
		}
		else
		{
			//	球の中心が箱の内側
			math::double3 relative;
			relative = (Point - Position).to_double3();
			const double* extent = &get_local_scale().x;
			const double* rel = &relative.x;

			//	最も浅い面の軸
			size_t axis_index = 1;
			double best = -1.0f;
			for (size_t index = 0; index < 3; ++index)
			{
				double ratio = (extent[index] > FLT_EPSILON) ? std::fabs(rel[index]) / extent[index] : FLT_MAX;
				if (ratio > best)
				{
					best = ratio;
					axis_index = index;
				}
			}

			double sign = rel[axis_index] >= 0.0f ? 1.0f : -1.0f;
			math::double3 normal(0.0f, 0.0f, 0.0f);
			(&normal.x)[axis_index] = -sign;

			math::double3 contact = point;
			(&contact.x)[axis_index] = (&get_position().x)[axis_index] + sign * extent[axis_index];
			intersect->add(contact, normal, radius + extent[axis_index] - std::fabs(rel[axis_index]));
		}
	}
	return true;
}

bool inner::axis_aligned_bounding_box::intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect) const
{
	return intersect_sphere(sphere.get_position().to_double3(), sphere.get_radius(), intersect);
}

bool inner::axis_aligned_bounding_box::intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect) const
{
	math::vector3d Direction = capsule.get_axis_vector();
	math::vector3d d = Direction * (capsule.get_length() * 0.5f);

	math::vector3d PositionA(get_position());
	math::vector3d PositionB(capsule.get_position());
	math::vector3d t = PositionB - PositionA;
	math::vector3d S1 = t - d;
	math::vector3d S2 = t + d;
	math::vector3d e(get_local_scale());

	double pen_depth = FLT_MAX;
	size_t pen_axis = 0;

	//	箱の3軸
	for (size_t index = 0; index < 3; ++index)
	{
		double projR = std::fabs(d[static_cast<int32_t>(index)]) + capsule.get_radius();
		double pd = (projR + e[static_cast<int32_t>(index)]) - std::fabs(t[static_cast<int32_t>(index)]);
		if (pd <= 0.0f)
			return false;
		if (pd < pen_depth)
		{
			pen_depth = pd;
			pen_axis = index;
		}
	}

	math::vector3d capep(0.0f, 0.0f, 0.0f);
	double adx = Direction.x;
	capep.x = adx < 0.0f ? adx - FLT_EPSILON : adx + FLT_EPSILON;
	double ady = Direction.y;
	capep.y = ady < 0.0f ? ady - FLT_EPSILON : ady + FLT_EPSILON;
	double adz = Direction.z;
	capep.z = adz < 0.0f ? adz - FLT_EPSILON : adz + FLT_EPSILON;

	math::vector3d Axes[3] =
	{
		math::vector3d(0.0f, -capep.z, capep.y),
		math::vector3d(capep.z, 0.0f, -capep.x),
		math::vector3d(-capep.y, capep.x, 0.0f),
	};

	for (size_t index = 0; index < 3; ++index)
	{
		double mag = Axes[index].length();
		double invMag = 1.0f / mag;
		Axes[index] = Axes[index] * invMag;

		double rb = e.x * std::fabs(Axes[index].x)
					+ e.y * std::fabs(Axes[index].y)
					+ e.z * std::fabs(Axes[index].z);
		double dist = std::fabs(math::vector3d::dot(Axes[index], t));

		double pd = (rb + capsule.get_radius()) - dist;
		if (pd <= 0.0f)
			return false;

		if (pd < pen_depth)
		{
			pen_depth = pd;
			pen_axis = 3 + index;
		}
	}

	if (intersect)
	{
		size_t begin = intersect->size();
		if (pen_axis < 3)
		{
			size_t clip_Axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

			for (size_t index = 0; index < 2; ++index)
			{
				size_t clip_axis = clip_Axes[pen_axis][index];
				double offset = (&get_local_scale().x)[clip_axis];

				if (S1[static_cast<int32_t>(clip_axis)] < offset && S2[static_cast<int32_t>(clip_axis)] > offset)
				{
					math::vector3d Delta = S2 - S1;
					double w = (offset - S1[static_cast<int32_t>(clip_axis)]) / Delta[static_cast<int32_t>(clip_axis)];
					S2 = S1 + (Delta * w);
				}
				else if (S1[static_cast<int32_t>(clip_axis)] > offset && S2[static_cast<int32_t>(clip_axis)] < offset)
				{
					math::vector3d Delta = S1 - S2;
					double w = (offset - S2[static_cast<int32_t>(clip_axis)]) / Delta[static_cast<int32_t>(clip_axis)];
					S1 = S2 + (Delta * w);
				}

				if (S1[static_cast<int32_t>(clip_axis)] > -offset && S2[static_cast<int32_t>(clip_axis)] < -offset)
				{
					math::vector3d Delta = S2 - S1;
					double w = (-offset - S1[static_cast<int32_t>(clip_axis)]) / Delta[static_cast<int32_t>(clip_axis)];
					S2 = S1 + (Delta * w);
				}
				else if (S1[static_cast<int32_t>(clip_axis)] < -offset && S2[static_cast<int32_t>(clip_axis)] > -offset)
				{
					math::vector3d Delta = S1 - S2;
					double w = (-offset - S2[static_cast<int32_t>(clip_axis)]) / Delta[static_cast<int32_t>(clip_axis)];
					S1 = S2 + (Delta * w);
				}
			}

			math::vector3d Normal(0.0f, 0.0f, 0.0f);
			double element = t[static_cast<int32_t>(pen_axis)] > 0.0f ? 1.0f : -1.0f;
			Normal[static_cast<int32_t>(pen_axis)] = element;

			//	法線方向に最も深い端点
			double diff = math::vector3d::dot(S1 - S2, Normal);

			math::vector3d Insert;
			if (fabs(diff) < FLT_EPSILON)
				Insert = (S1 + S2) * 0.5f;
			else if (diff < 0.0f)
				Insert = S1;
			else
				Insert = S2;
			Insert = Insert - (Normal * capsule.get_radius());
			Insert = Insert + math::vector3d(get_position());

			math::double3 insert, normal;
			insert = Insert.to_double3();
			normal = Normal.to_double3();
			intersect->add(insert, normal, pen_depth);
		}
		else
		{
			pen_axis -= 3;
			if (math::vector3d::dot(t, Axes[pen_axis]) < 0.0f)
			{
				Axes[pen_axis] = -Axes[pen_axis];
			}

			//	カプセル側へ向いた箱の辺
			math::double3 p1(0.0f, 0.0f, 0.0f), q1(0.0f, 0.0f, 0.0f);
			for (size_t index = 0; index < 3; ++index)
			{
				double extent = e[static_cast<int32_t>(index)];
				if (index == pen_axis)
				{
					(&p1.x)[index] = +extent;
					(&q1.x)[index] = -extent;
				}
				else
				{
					double value = Axes[pen_axis][static_cast<int32_t>(index)] < 0.0f ? -extent : extent;
					(&p1.x)[index] = value;
					(&q1.x)[index] = value;
				}
			}

			math::double3 s1, s2;
			s1 = S1.to_double3();
			s2 = S2.to_double3();
			lined line1(lined::types::segment, s1, s2);
			lined line2(lined::types::segment, p1, q1);

			math::double3 cp_aabb, cp_cap;
			double length_aabb, length_cap;
			line1.calculate_distance(cp_cap, length_cap, cp_aabb, length_aabb, line2);

			math::vector3d CP_CAP(cp_cap);
			math::vector3d P1 = CP_CAP - (Axes[pen_axis] * capsule.get_radius());
			P1 = P1 + math::vector3d(get_position());
			p1 = P1.to_double3();

			math::double3 axes;
			axes = math::vector3d(Axes[pen_axis]).to_double3();
			intersect->add(p1, axes, pen_depth);
		}

		//	箱を押し出す向きへ反転
		intersect->flip_normals_from(begin);
	}

	return true;
}

bool inner::axis_aligned_bounding_box::intersect_box(const bounding_box& box, intersect_data* intersect) const
{
	(void)intersect;
	return bounding_box(get_local_scale(), math::double4(0.0f, 0.0f, 0.0f, 1.0f), get_position().to_double3()).intersect_box(box, nullptr);
}

bool inner::axis_aligned_bounding_box::intersect_box(const axis_aligned_bounding_box& box) const
{
	math::vector3d CenterA(get_position());
	math::vector3d ExtentsA(get_local_scale());

	math::vector3d CenterB(box.get_position());
	math::vector3d ExtentsB(box.get_local_scale());

	math::vector3d MinA = CenterA - ExtentsA;
	math::vector3d MaxA = CenterA + ExtentsA;

	math::vector3d MinB = CenterB - ExtentsB;
	math::vector3d MaxB = CenterB + ExtentsB;

	return !(math::any_greater(MinA, MaxB) || math::any_greater(MinB, MaxA));
}

bool inner::axis_aligned_bounding_box::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	//	未対応
	return false;
}

bool inner::axis_aligned_bounding_box::intersect_sphere_cast(const math::double3& start, const math::double3& end, double radius, intersect_data* intersect) const
{
	math::vector3d Start(start);
	math::vector3d End(end);

	return intersect_sphere_cast(Start, End, radius, intersect);
}

bool inner::axis_aligned_bounding_box::intersect_sphere_cast(math::vector3d Start, math::vector3d End, double radius, intersect_data* intersect) const
{
	math::vector3d Position(get_position());
	math::vector3d Extents(get_local_scale());

	//	スフィアキャストの始端、AABBの中心、AABBとスフィアキャスト半径のボロノイ領域の半径を
	//	ループ処理するため、xyz成分を配列に代入する
	double start_pos_array[3] = { Start.x, Start.y, Start.z };
	double aabb_pos_array[3] = { Position.x, Position.y, Position.z };
	double aabb_extent_array[3] = { Extents.x, Extents.y, Extents.z };
	double aabb_voronoi_rad_array[3] =
	{
		aabb_extent_array[0] + radius,
		aabb_extent_array[1] + radius,
		aabb_extent_array[2] + radius
	};

	//	スフィアキャストのベクトルを作り、距離を変数にバックアップしたうえで正規化し、
	//	こちらもループ処理するため、xyz成分を配列に代入
	math::vector3d DVec = End - Start;
	double ray_length = DVec.length();
	if (ray_length <= 1e-6f)
		return false;
	DVec = DVec.normalized();
	double d_array[3] = { DVec.x, DVec.y, DVec.z };

	//	開始時の重なり
	{
		math::double3 closest;
		double* closest_array = &closest.x;
		for (int i = 0; i < 3; ++i)
			closest_array[i] = math::clamp(start_pos_array[i], aabb_pos_array[i] - aabb_extent_array[i], aabb_pos_array[i] + aabb_extent_array[i]);
		math::vector3d Closest(closest);
		math::vector3d Vector = Start - Closest;
		double distance = Vector.length();
		if (distance < radius)
		{
			math::vector3d Normal;
			if (distance > 1e-6f)
			{
				Normal = Vector * (1.0f / distance);
			}
			else
			{
				//	中心が箱の内側なら最も浅い面
				size_t axis_index = 1;
				double best = -1.0f;
				for (size_t i = 0; i < 3; ++i)
				{
					double relative = start_pos_array[i] - aabb_pos_array[i];
					double ratio = (aabb_extent_array[i] > FLT_EPSILON) ? std::fabs(relative) / aabb_extent_array[i] : FLT_MAX;
					if (ratio > best)
					{
						best = ratio;
						axis_index = i;
					}
				}
				math::double3 normal(0.0f, 0.0f, 0.0f);
				(&normal.x)[axis_index] = (start_pos_array[axis_index] - aabb_pos_array[axis_index]) >= 0.0f ? 1.0f : -1.0f;
				Normal = math::vector3d(normal);
			}
			if (math::vector3d::dot(DVec, Normal) >= 0.0f)
				return false;
			add_cast_hit(intersect, Start, Closest, Normal, 0.0f);
			return true;
		}
	}

	//	直線とスラブの２交点までの距離をtminとtmaxと定義
	double tmin = 0.0f;
	double tmax = FLT_MAX;

	//	スラブとの距離を算出し交差しているかの確認と最近点の算出を行う
	for (int i = 0; i < 3; i++)
	{
		//	xyz軸との平行確認
		if (std::fabs(d_array[i]) < FLT_EPSILON)
		{
			//	平行の場合、位置関係の比較を行い、範囲内になければ交差なしでreturn false
			if (start_pos_array[i] < aabb_pos_array[i] - aabb_voronoi_rad_array[i] ||
				start_pos_array[i] > aabb_pos_array[i] + aabb_voronoi_rad_array[i])
			{
				return false;
			}
		}
		else
		{
			//	t1が近スラブ、t2が遠スラブとの距離
			double ood = 1.0f / d_array[i];
			double t1 = (aabb_pos_array[i] - aabb_voronoi_rad_array[i] - start_pos_array[i]) * ood;
			double t2 = (aabb_pos_array[i] + aabb_voronoi_rad_array[i] - start_pos_array[i]) * ood;

			//	遠近が逆転している場合があるので、その場合入れ替えておく
			if (t1 > t2)
			{
				double tmp = t1;
				t1 = t2;
				t2 = tmp;
			}

			//	t1がtminよりも大きい場合、tminをt1で更新する
			if (t1 > tmin) tmin = t1;

			//	t2がtmaxよりも小さい場合、tmaxをt2で更新する
			if (t2 < tmax) tmax = t2;

			//	tminとtmaxの大小関係が逆転するのは、交差していない場合のみなので、その場合はreturn false
			if (tmin > tmax) return false;
		}
	}

	//	スラブとの交点がレイの外側にある
	if (ray_length < tmin) return false;

	//	ここまで来たらスフィアキャストと拡大したAABB（ボロノイ領域）との交差が確定
	//	交点を割り出す。
	math::double3 point =
	{
		start_pos_array[0] + d_array[0] * tmin,
		start_pos_array[1] + d_array[1] * tmin,
		start_pos_array[2] + d_array[2] * tmin,
	};

	//	求めた交点から、頂点・面・辺のボロノイ領域のチェックを行う
	//	u をマイナス、v をプラスとし、ビットでxyzの向きを管理している
	int u = 0, v = 0;
	if (point.x <= aabb_pos_array[0] - Extents.x) u |= (1 << 0);
	else if (point.x >= aabb_pos_array[0] + Extents.x) v |= (1 << 0);
	if (point.y <= aabb_pos_array[1] - Extents.y) u |= (1 << 1);
	else if (point.y >= aabb_pos_array[1] + Extents.y) v |= (1 << 1);
	if (point.z <= aabb_pos_array[2] - Extents.z) u |= (1 << 2);
	else if (point.z >= aabb_pos_array[2] + Extents.z) v |= (1 << 2);

	int mask = u | v;

	if (mask != 0 && (mask & (mask - 1)) == 0)	//	ボロノイ面領域の場合
	{
		//	面の外向き法線
		math::vector3d Normal = math::vector3d((double)(((v & 1) - (u & 1)) >> 0), (double)(((v & 2) - (u & 2)) >> 1), (double)(((v & 4) - (u & 4)) >> 2));

		math::vector3d Center(point);
		math::vector3d Contact = Center - (Normal * radius);
		add_cast_hit(intersect, Center, Contact, Normal, tmin);
		return true;
	}

	//	辺・頂点領域は全頂点の球と全辺の円柱で最も近い交点を求める
	math::vector3d NearPos = {};
	double near_dist = FLT_MAX;

	math::vector3d HitPosition;
	double hit_distance;
	for (int corner = 0; corner < 8; ++corner)
	{
		math::vector3d Vertex = get_aabb_corner(Position, Extents, corner);
		if (collision::intersect_ray_vs_sphere(Start, End, Vertex, radius, HitPosition, hit_distance) && hit_distance > 0.0f && hit_distance < near_dist)
		{
			NearPos = Vertex;
			near_dist = hit_distance;
		}
	}
	for (int corner = 0; corner < 8; ++corner)
	{
		for (int bit = 1; bit <= 4; bit <<= 1)
		{
			if (corner & bit)
				continue;

			math::vector3d StartVertex = get_aabb_corner(Position, Extents, corner);
			math::vector3d EndVertex = get_aabb_corner(Position, Extents, corner | bit);
			if (collision::intersect_ray_vs_cylinder(Start, End, StartVertex, EndVertex, radius, HitPosition, hit_distance) && hit_distance > 0.0f && hit_distance < near_dist)
			{
				math::vector3d Vec = HitPosition - StartVertex;
				math::vector3d EdgeVec = (EndVertex - StartVertex).normalized();
				NearPos = (math::vector3d::dot(Vec, EdgeVec)) * EdgeVec + StartVertex;
				near_dist = hit_distance;
			}
		}
	}

	if (near_dist == FLT_MAX)
		return false;

	//	接触点（AABB側の頂点/辺上の最近点）から、実際に球が接触した位置へ法線を求める
	math::vector3d Center = DVec * (math::vector3d(near_dist, near_dist, near_dist)) + Start;
	math::vector3d Delta = Center - NearPos;
	double delta_length = Delta.length();
	math::vector3d Normal = (delta_length > 1e-6f) ? Delta * (1.0f / delta_length) : -DVec;
	add_cast_hit(intersect, Center, NearPos, Normal, near_dist);
	return true;
}

bool inner::axis_aligned_bounding_box::intersect_capsule_cast(const math::double3& start, const math::double3& end,
															   double radius, double length, const math::double4& rotation,
															   intersect_data* intersect) const
{
	math::vector3d axis = math::quaterniond(rotation).rotate(math::vector3d(0.0f, 1.0f, 0.0f));
	return intersect_capsule_cast_by_axis_spheres(start, end, radius, length, axis, intersect);
}

void inner::axis_aligned_bounding_box::include_corner(math::double3& min, math::double3& max) const
{
	math::vector3d Min, Max;
	include_corner(Min, Max);
	min = Min.to_double3();
	max = Max.to_double3();
}

void inner::axis_aligned_bounding_box::include_corner(math::vector3d& Min, math::vector3d& Max) const
{
	math::vector3d Position(get_position());
	math::vector3d Scale(get_local_scale());
	Min = Position - Scale;
	Max = Position + Scale;
}

void inner::axis_aligned_bounding_box::calculate_vertex(math::double3* vertices, size_t vertex_count) const
{
	if (!vertices)
		return;

	math::vector3d P(get_position());
	math::vector3d X(get_local_scale().x, 0.0f, 0.0f);
	math::vector3d Y(0.0f, get_local_scale().y, 0.0f);
	math::vector3d Z(0.0f, 0.0f, get_local_scale().z);
	math::vector3d AddPpX = P + X;
	math::vector3d AddPnX = P - X;
	math::vector3d AddYpZ = Y + Z;
	math::vector3d AddYnZ = Y - Z;
	vertices[0] = (AddPnX + AddYnZ).to_double3();
	vertices[1] = (AddPpX + AddYnZ).to_double3();
	vertices[2] = (AddPnX + AddYpZ).to_double3();
	vertices[3] = (AddPpX + AddYpZ).to_double3();

	vertices[4] = (AddPnX - AddYnZ).to_double3();
	vertices[5] = (AddPpX - AddYnZ).to_double3();
	vertices[6] = (AddPnX - AddYpZ).to_double3();
	vertices[7] = (AddPpX - AddYpZ).to_double3();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
	static constexpr size_t TriFace[3 * 2 * 6] =
	{
		0, 1, 2,	1, 3, 2,	//!<	上
		4, 6, 5,	5, 6, 7,	//!<	下
		0, 4, 1,	1, 4, 5,	//!<	左
		1, 5, 3,	3, 5, 7,	//!<	後
		3, 6, 2,	3, 7, 6,	//!<	右
		2, 6, 0,	0, 6, 4		//!<	前
	};

	static constexpr size_t PlaneFace[4 * 6] =
	{
		0, 1, 2, 3,	//!<	上
		4, 6, 5, 7,	//!<	下
		0, 4, 1, 5,	//!<	左
		1, 5, 3, 7,	//!<	後
		3, 6, 2, 7,	//!<	右
		2, 6, 0, 4,	//!<	前
	};

	void	clipPolygonToPlaneNegative(const math::vector3d& planeN, double planeOffset,
									   const math::vector3d* verts, size_t vertCount,
									   math::vector3d* out, size_t& outCount)
	{
		constexpr size_t START_IN = 0x0;
		constexpr size_t START_ON = 0x1;
		constexpr size_t START_OUT = 0x2;

		constexpr size_t END_IN = 0x0;
		constexpr size_t END_ON = 0x4;
		constexpr size_t END_OUT = 0x8;

		outCount = 0;

		switch (std::min(vertCount, static_cast<size_t>(3)))
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				double offset = math::vector3d::dot(verts[0], planeN) - planeOffset;
				if (offset <= FLT_EPSILON)
				{
					out[0] = verts[0];
					outCount = 1;
				}
				break;
			}
			case 2:
			{
				const math::vector3d& start = verts[0];
				const math::vector3d& end = verts[1];
			
				double startOffset = math::vector3d::dot(start, planeN) - planeOffset;
				double endOffset = math::vector3d::dot(end, planeN) - planeOffset;

				size_t result = START_IN | END_IN;

				if( fabs(startOffset) <= FLT_EPSILON )
					result |= START_ON;
				else if( startOffset > FLT_EPSILON )
					result |= START_OUT;

				if( fabs(endOffset) <= FLT_EPSILON )
					result |= END_ON;
				else if( endOffset > FLT_EPSILON )
					result |= END_OUT;

				switch (result)
				{
					case (START_IN | END_OUT):
					{
						math::vector3d edge = end - start;
						double d = -startOffset / math::vector3d::dot(edge, planeN);

						out[0] = start;
						out[1] = start + (edge * d);
						outCount = 2;
						break;				
					}
					case (START_OUT | END_IN):
					{
						math::vector3d edge = end - start;
						double d = -startOffset / math::vector3d::dot(edge, planeN);

						out[0] = start + (edge * d);
						out[1] = end;
						outCount = 2;
					}
					case ( START_OUT | END_OUT ):
					{
						break;
					}
					case ( START_ON | END_OUT ):
					{
						out[0] = start;
						outCount = 1;
						break;
					}
					case ( START_OUT | END_ON ):
					{
						out[0] = end;
						outCount = 1;
					}
					break;
					default:
					{
						out[0] = start;
						out[1] = end;
						outCount = 2;
					}
					break;
				}	
				break;	
			}
			default:
			{
				math::vector3d start = verts[vertCount-1];

				for (size_t index = 0; index < vertCount; ++index)
				{
					const math::vector3d& end = verts[index];

					double startOffset = math::vector3d::dot(start, planeN) - planeOffset;	
					double endOffset = math::vector3d::dot(end, planeN) - planeOffset;

					size_t result = START_IN | END_IN;

					if (abs(startOffset) <= FLT_EPSILON)
						result |= START_ON;
					else if (startOffset > FLT_EPSILON)
						result |= START_OUT;

					if (abs(endOffset) <= FLT_EPSILON)
						result |= END_ON;
					else if (endOffset > FLT_EPSILON)
						result |= END_OUT;

					switch (result)
					{
						case (START_IN | END_OUT):
						{
							math::vector3d edge = end - start;
							double d = -startOffset / math::vector3d::dot(edge, planeN);

							out[outCount] = start + (edge * d);
							++outCount;					
						}
						break;
						case (START_OUT | END_IN):
						{
							math::vector3d edge = end - start;
							double d = -startOffset / math::vector3d::dot(edge, planeN);

							out[outCount] = start + (edge * d);
							++outCount;	

							out[outCount] = end;
							++outCount;
						}
						case ( START_OUT | END_OUT ):
						case ( START_ON | END_OUT ):
						{
							break;
						}
						default:
						{
							out[outCount] = end;
							++outCount;
						}
						break;
					}

					start = end;
				}
			}
			break;
		}
	}

	bool face_contact_obb_to_aabb(const bounding_box& obb, const inner::axis_aligned_bounding_box& aabb, size_t axis_index, math::vector3d& InsertPoint)
	{
		math::matrixd ObbMatrix = obb.get_transeform_matrix();
		math::vector3d Axis(0.0f, 0.0f, 0.0f);
		math::vector3d Delta = ObbMatrix.to_vector3(3) - math::vector3d(aabb.get_position());
	
		Axis[static_cast<int32_t>(axis_index)] = Delta[static_cast<int32_t>(axis_index)] < 0.0f ? -1.0f : 1.0f;

		math::vector3d LocalAxis = math::vector3d::transform_normal(Axis, ObbMatrix.transposed());

		math::vector3d ScaledAxis = -LocalAxis / math::vector3d(obb.get_local_scale());

		size_t face_axis = 0;
		if (fabs(ScaledAxis.x) < fabs(ScaledAxis.y))
		{
			face_axis = 1;
		}
		if (fabs(ScaledAxis[static_cast<int32_t>(face_axis)]) < fabs(ScaledAxis.z))
		{
			face_axis = 2;
		}

		math::vector3d Vertices[12];
		size_t face_index = ScaledAxis[static_cast<int32_t>(face_axis)] < 0.0f ? face_axis : face_axis + 3;

		static const math::vector3d UnitVertices[8] =
		{
			math::vector3d(-1.0f, -1.0f, -1.0f),
			math::vector3d(-1.0f, -1.0f, 1.0f),
			math::vector3d(-1.0f, 1.0f, -1.0f),
			math::vector3d(-1.0f, 1.0f, 1.0f),
			math::vector3d(1.0f, -1.0f, -1.0f),
			math::vector3d(1.0f, -1.0f, 1.0f),
			math::vector3d(1.0f, 1.0f, -1.0f),
			math::vector3d(1.0f, 1.0f, 1.0f)
		};
		static const size_t UnitPlaneFace[4 * 6] =
		{
			0, 1, 3, 2,	//	上
			1, 0, 4, 5,	//	前
			0, 2, 6, 4,	//	左
			4, 6, 7, 5,	//	下
			2, 3, 7, 6,	//	後
			1, 5, 7, 3,	//	右
		};
		for (size_t index = 0; index < 4; index++)
		{
			Vertices[index] = (UnitVertices[UnitPlaneFace[face_index * 4 + index]]) * math::vector3d(obb.get_local_scale());

			Vertices[index] = math::vector3d::transform_coord(Vertices[index], ObbMatrix);
		}

		size_t clip_axes[][2]	=
		{
			{ 1, 2 },
			{ 0, 2 },
			{ 0, 1 }
		};

		math::vector3d Buffer[12];
		math::vector3d* Input = Vertices;
		math::vector3d* Output = Buffer;
		size_t input_count = 4;
		size_t output_count = 0;
		for(int a = 0; a < 2; ++a)
		{
			size_t index = clip_axes[axis_index][a];

			math::vector3d PlaneN(0.0f, 0.0f, 0.0f);
			PlaneN[static_cast<int32_t>(index)] = 1.0f;
			double planeOffset = (&aabb.get_local_scale().x)[index];
			
			clipPolygonToPlaneNegative(PlaneN, planeOffset, Input, input_count, Output, output_count);

			input_count = output_count;
			output_count = 0;
			math::vector3d* Temp = Input;
			Input = Output;
			Output = Temp;

			PlaneN[static_cast<int32_t>(index)] = -1.0f;
			clipPolygonToPlaneNegative(PlaneN, planeOffset, Input, input_count, Output, output_count);

			input_count = output_count;
			output_count = 0;
			Temp = Input;
			Input = Output;
			Output = Temp;
		}

		if (input_count == 0)
			return false;

		int size = (int)input_count;
		double distances[12];
		for (size_t index = 0; index < size; ++index)
		{
			distances[index] = Input[index][static_cast<int32_t>(axis_index)] * Axis[static_cast<int32_t>(axis_index)];
		}

		for (size_t loop = 0; loop < size; ++loop)
		{
			for (size_t index = 0; index < size - 1; ++index)
			{
				if (distances[index] > distances[index + 1])
				{
					std::swap(distances[index], distances[index + 1]);
					std::swap(Input[index], Input[index + 1]);
				}
			}
		}

		math::vector3d Average(0.0f, 0.0f, 0.0f);
		size_t count = 0;
		for (; count < input_count; ++count)
		{
			if (std::fabs(distances[0] - distances[count]) > 0.01f)
				break;

			Average = Average + Input[count];
		}

		Average = Average / static_cast<double>(count);
		InsertPoint = Average + math::vector3d(aabb.get_position());
		return true;
	}
}	//	namespace

////////////////////////////////////////////////////////////////////////////////////////////
math::matrixd bounding_box::get_transeform_matrix() const
{
	return	get_transform_matrix();
}

math::double4x4 bounding_box::get_transeform_float4x4() const
{
	math::double4x4 matrix;
	matrix = get_transeform_matrix().to_double4x4();
	return matrix;
}

bounding_box::bounding_box()
: bounding_object()
{
}

bounding_box::bounding_box(const math::double3& scale, const math::double4& rotation, const math::double3& position)
: bounding_object(scale, rotation, position)
{
}

bounding_box::bounding_box(const math::double3& scale, const math::double3& y_axis, double angle, const math::double3& position)
: bounding_object(scale, math::double4(0, 0, 0, 1), position)
{
	math::vector3d YAxis(y_axis);
	math::quaterniond Rotation = math::quaterniond::rotation_axis(YAxis, angle);
	math::double4 rotation;
	rotation = Rotation.to_double4();
	set_rotation(rotation);
}

bounding_box::~bounding_box()
{
}

double bounding_box::calculate_point_to_length(const math::double3& point) const
{
	math::vector3d Vector(0, 0, 0);   // 最終的に長さを求めるベクトル

	math::vector3d Point(point);
	math::vector3d PositionObb(get_position());
	math::matrixd Rotation = math::matrixd::rotation_quaternion(math::quaterniond(get_rotation()));

	//	各軸についてはみ出た部分のベクトルを算出
	const double* length = &get_local_scale().x;
	for (int axis = 0; axis < 3; axis++)
	{
		if (length[axis] <= 0)
			continue;	//	L = 0は計算できない

		math::vector3d PointToPositionObb = Point - PositionObb;
		const math::vector3d ObbAxis = Rotation.to_vector3(axis);
		double s = math::vector3d::dot(PointToPositionObb, ObbAxis) / length[axis];

		//	sの値から、はみ出した部分があればそのベクトルを加算
		s = std::fabs(s);
		if (s > 1)
		{
			s = (1 - s) * length[axis];
			//	はみ出した部分のベクトル算出
			Vector = Vector + ObbAxis * s;
		}
	}

	//	長さを出力
	return	Vector.length();
}

bool bounding_box::intersect_point(const math::double3& point) const
{
	return calculate_point_to_length(point) <= FLT_EPSILON;
}

bool bounding_box::intersect_segment(const math::double3& start, const math::double3& end, intersect_data* intersect) const
{
	//	ローカル空間のAABBで判定
	math::double3 local_start, local_end;
	local_start = (math::vector3d::transform_coord(math::vector3d(start), get_inverse_transform_matrix())).to_double3();
	local_end = (math::vector3d::transform_coord(math::vector3d(end), get_inverse_transform_matrix())).to_double3();

	size_t begin = intersect ? intersect->size() : 0;
	inner::axis_aligned_bounding_box aabb(get_local_scale(), {0.0f, 0.0f, 0.0f});
	bool contact = aabb.intersect_segment(local_start, local_end, intersect);
	if (contact && intersect)
		intersect->multiply_matrices(begin, get_transeform_matrix());
	return contact;
}

bool bounding_box::intersect_sphere(const math::double3& point, double radius, intersect_data* intersect) const
{
	math::vector3d LocalPoint;
	LocalPoint = math::vector3d::transform_coord(math::vector3d(point), get_inverse_transform_matrix());

	math::double3 new_point;
	new_point = LocalPoint.to_double3();

	size_t begin = intersect ? intersect->size() : 0;
	inner::axis_aligned_bounding_box aabb(get_local_scale(), {0.0f, 0.0f, 0.0f});
	bool contact = aabb.intersect_sphere(new_point, radius, intersect);
	if (contact && intersect)
		intersect->multiply_matrices(begin, get_transeform_matrix());
	return contact;
}

bool bounding_box::intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect) const
{
	return	intersect_sphere(sphere.get_position().to_double3(), sphere.get_radius(), intersect);
}

bool bounding_box::intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect) const
{
	math::matrixd InverseCapsuleRotationMatrix;
	InverseCapsuleRotationMatrix = capsule.get_transform_matrix() * get_inverse_transform_matrix();

	math::vector3d Dummy, LocalCapsulePosition;
	math::quaterniond LocalCapsuleRotation;
	InverseCapsuleRotationMatrix.decompose(Dummy, LocalCapsuleRotation, LocalCapsulePosition);

	math::double3 local_capsule_position;
	local_capsule_position = LocalCapsulePosition.to_double3();
	math::double4 local_rotation;
	local_rotation = LocalCapsuleRotation.to_double4();
	bounding_capsule local_capsule(local_capsule_position, local_rotation, capsule.get_radius(), capsule.get_length());

	size_t begin = intersect ? intersect->size() : 0;
	inner::axis_aligned_bounding_box aabb(get_local_scale(), {0.0f, 0.0f, 0.0f});
	bool contact = aabb.intersect_capsule(local_capsule, intersect);
	if (contact && intersect)
		intersect->multiply_matrices(begin, get_transeform_matrix());
	return contact;
}

bool bounding_box::intersect_box(const bounding_box& box, intersect_data* intersect) const
{
	math::matrixd xformA = get_transeform_matrix();
	math::vector3d extentsA(get_local_scale());

	math::matrixd xformB = box.get_transeform_matrix();
	math::vector3d extentsB(box.get_local_scale());

	math::double3x3 r;
	r.m[0][0] = math::vector3d::dot(xformA.to_vector3(0), xformB.to_vector3(0));
	r.m[0][1] = math::vector3d::dot(xformA.to_vector3(0), xformB.to_vector3(1));
	r.m[0][2] = math::vector3d::dot(xformA.to_vector3(0), xformB.to_vector3(2));

	r.m[1][0] = math::vector3d::dot(xformA.to_vector3(1), xformB.to_vector3(0));
	r.m[1][1] = math::vector3d::dot(xformA.to_vector3(1), xformB.to_vector3(1));
	r.m[1][2] = math::vector3d::dot(xformA.to_vector3(1), xformB.to_vector3(2));

	r.m[2][0] = math::vector3d::dot(xformA.to_vector3(2), xformB.to_vector3(0));
	r.m[2][1] = math::vector3d::dot(xformA.to_vector3(2), xformB.to_vector3(1));
	r.m[2][2] = math::vector3d::dot(xformA.to_vector3(2), xformB.to_vector3(2));

	math::vector3d t = xformB.to_vector3(3) - xformA.to_vector3(3);
	t = math::vector3d(math::vector3d::dot(t, xformA.to_vector3(0)), math::vector3d::dot(t, xformA.to_vector3(1)), math::vector3d::dot(t, xformA.to_vector3(2)));

	math::double3x3 abs_r;
	for(int i = 0; i < 3; ++i)
	{
		for(int j = 0; j < 3; ++j)
		{
			abs_r.m[i][j] = abs( r.m[i][j] ) + FLT_EPSILON;
		}
	}

	double penetration_depth = FLT_MAX;
	math::double3 axis(0.0f, 0.0f, 0.0f);
	size_t world_axis = 0;
	for (size_t index = 0; index < 3; ++index)
	{
		double	ra	= extentsA[static_cast<int32_t>(index)];
		double	rb	= extentsB[0] * abs_r.m[index][0]
					+ extentsB[1] * abs_r.m[index][1]
					+ extentsB[2] * abs_r.m[index][2];

		double d = std::fabs(t[static_cast<int32_t>(index)]);

		double pd = (ra + rb) - d;
			
		if (pd <= 0.0f)
			return false;

		if (pd < penetration_depth)
		{
			penetration_depth = pd;
			axis = math::double3(0.0f, 0.0f, 0.0f);
			(&axis.x)[index] = 1.0f;
			world_axis = index;
		}
	}

	for (size_t index = 0; index < 3; ++index)
	{
		double ra = extentsA.x * abs_r.m[0][index]
				 + extentsA.y * abs_r.m[1][index]
				 + extentsA.z * abs_r.m[2][index];
		double rb = extentsB[static_cast<int32_t>(index)];

		double d = std::fabs(t.x * r.m[0][index] + t.y * r.m[1][index] + t.z * r.m[2][index]);
		double pd = (ra + rb) - d;

		if (pd <= 0.0f)
			return false;

		if (pd < penetration_depth)
		{
			penetration_depth = pd;
			axis = math::double3(r.m[0][index], r.m[1][index], r.m[2][index]);
			world_axis = index + 3;
		}
	}

	math::double3 axes[9];
	double pds[9] = {0.0f};

	for (size_t indexA = 0; indexA < 3; ++indexA)
	{
		math::double3 axisA(0.0f, 0.0f, 0.0f);
		(&axisA.x)[indexA] = 1.0f;

		for (size_t indexB = 0; indexB < 3; ++indexB)
		{
			math::double3 axisB(r.m[0][indexB], r.m[1][indexB], r.m[2][indexB]);

			axisB.x += (axisB.x < 0.0f) ? -FLT_EPSILON : FLT_EPSILON;
			axisB.y += (axisB.y < 0.0f) ? -FLT_EPSILON : FLT_EPSILON;
			axisB.z += (axisB.z < 0.0f) ? -FLT_EPSILON : FLT_EPSILON;

			math::double3& current_axis = axes[indexA * 3 + indexB];
			math::vector3d AxisA(axisA);
			math::vector3d AxisB(axisB);
			current_axis = (math::vector3d::cross(AxisA, AxisB)).to_double3();

			double ra = extentsA.x * std::fabs(current_axis.x)
					 + extentsA.y * std::fabs(current_axis.y)
					 + extentsA.z * std::fabs(current_axis.z);

			double rb = extentsB.x * std::fabs(current_axis.x * r.m[0][0] + current_axis.y * r.m[1][0] + current_axis.z * r.m[2][0])
					 + extentsB.y * std::fabs(current_axis.x * r.m[0][1] + current_axis.y * r.m[1][1] + current_axis.z * r.m[2][1])
					 + extentsB.z * std::fabs(current_axis.x * r.m[0][2] + current_axis.y * r.m[1][2] + current_axis.z * r.m[2][2]);

			double& pd = pds[indexA * 3 + indexB];

			pd = (ra + rb) - std::fabs(math::vector3d::dot(t, math::vector3d(current_axis)));

			if (pd <= 0.0f)
				return false;
		}
	}

	if (intersect)
	{
		size_t axisA = -1;
		size_t axisB = -1;
		for (size_t indexA = 0; indexA < 3; ++indexA)
		{
			for (size_t indexB = 0; indexB < 3; ++indexB)
			{
				size_t index = indexA * 3 + indexB;

				math::double3& current_axis = axes[index];
				double& pd = pds[index];

				double mag = math::vector3d(current_axis).length();

				if (mag < FLT_EPSILON)
					continue;

				double inv_mag = 1.0f / mag;
				pd *= inv_mag;

				//	浅すぎる辺軸は採用しない
				if (pd <= 0.01f)
					continue;

				if (pd < penetration_depth)
				{
					penetration_depth = pd;
					axis.x = current_axis.x * inv_mag;
					axis.y = current_axis.y * inv_mag;
					axis.z = current_axis.z * inv_mag;
					axisA = indexA;
					axisB = indexB;
				}
			}
		}

		if (math::vector3d::dot(math::vector3d(axis), t) > 0.0f)
		{
			axis = (-math::vector3d(axis)).to_double3();
		}
			
		//	ワールド法線
		math::vector3d NormalWorld = (math::vector3d::transform_normal(math::vector3d(axis), xformA)).normalized();

		//	指定方向に最も突き出た頂点
		auto support_point = [](const bounding_box& target, math::vector3d Direction)
		{
			math::double3 vertices[8];
			target.calculate_vertex(vertices, 8);
			math::vector3d Result(vertices[0]);
			double best = math::vector3d::dot(Result, Direction);
			for (size_t index = 1; index < 8; ++index)
			{
				math::vector3d Vertex(vertices[index]);
				double value = math::vector3d::dot(Vertex, Direction);
				if (value > best)
				{
					best = value;
					Result = Vertex;
				}
			}
			return Result;
		};

		//	線と線
		if (axisA != -1)
		{
			//	指定方向側の辺の両端(ローカル)
			auto edge_points = [](math::vector3d Direction, const math::double3& extent, size_t edge_axis, math::vector3d& P, math::vector3d& Q)
			{
				math::double3 p(0.0f, 0.0f, 0.0f), q(0.0f, 0.0f, 0.0f);
				for (size_t index = 0; index < 3; ++index)
				{
					double value = (&extent.x)[index];
					if (index == edge_axis)
					{
						(&p.x)[index] = +value;
						(&q.x)[index] = -value;
					}
					else
					{
						value = Direction[static_cast<int32_t>(index)] < 0.0f ? -value : value;
						(&p.x)[index] = value;
						(&q.x)[index] = value;
					}
				}
				P = math::vector3d(p);
				Q = math::vector3d(q);
			};

			//	AはB側(-axis)、BはA側(+axis)の辺
			math::vector3d PA, QA, PB, QB;
			edge_points(-math::vector3d(axis), get_local_scale(), axisA, PA, QA);
			math::vector3d AxisLocalB = math::vector3d::transform_normal(NormalWorld, xformB.transposed());
			edge_points(AxisLocalB, box.get_local_scale(), axisB, PB, QB);

			//	Bのローカル→Aのローカル
			math::matrixd InverseA = get_inverse_transform_matrix();
			PB = math::vector3d::transform_coord(math::vector3d::transform_coord(PB, xformB), InverseA);
			QB = math::vector3d::transform_coord(math::vector3d::transform_coord(QB, xformB), InverseA);

			math::double3 pA, pB, qA, qB;
			pA = PA.to_double3();
			pB = PB.to_double3();
			qA = QA.to_double3();
			qB = QB.to_double3();

			lined line1(lined::types::segment, pA, qA);
			lined line2(lined::types::segment, pB, qB);

			math::double3 cpA, cpB;
			double deltaA, deltaB;
			line1.calculate_distance(cpA, deltaA, cpB, deltaB, line2);

			//	両辺の最近点の中点
			math::vector3d Contact = (math::vector3d(cpA) + math::vector3d(cpB)) * 0.5f;
			math::double3 contact, normal;
			contact = (math::vector3d::transform_coord(Contact, xformA)).to_double3();
			normal = NormalWorld.to_double3();
			intersect->add(contact, normal, penetration_depth);
		}
		else	//	線と面 / 面と面
		{
			math::vector3d ContactPt;
			if (world_axis < 3)
			{
				//	Aの面にめり込んだBの頂点
				bounding_box local_obb = box.multiply_obb(get_transeform_matrix());
				inner::axis_aligned_bounding_box local_aabb(get_local_scale(), math::double3(0.0f, 0.0f, 0.0f));

				if (face_contact_obb_to_aabb(local_obb, local_aabb, world_axis, ContactPt))
					ContactPt = math::vector3d::transform_coord(ContactPt, get_transeform_matrix());
				else
					ContactPt = support_point(box, NormalWorld);
			}
			else
			{
				//	Bの面にめり込んだAの頂点
				bounding_box local_obb = multiply_obb(box.get_transeform_matrix());
				inner::axis_aligned_bounding_box local_aabb(box.get_local_scale(), math::double3(0.0f, 0.0f, 0.0f));

				if (face_contact_obb_to_aabb(local_obb, local_aabb, world_axis - 3, ContactPt))
					ContactPt = math::vector3d::transform_coord(ContactPt, box.get_transeform_matrix());
				else
					ContactPt = support_point(*this, -NormalWorld);
			}

			math::double3 contactPt, normal;
			contactPt = ContactPt.to_double3();
			normal = NormalWorld.to_double3();
			intersect->add(contactPt, normal, penetration_depth);
		}
	}
	return true;
}

bool bounding_box::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	return delegate_flipped(intersect, [&]() { return mesh.intersect_box(*this, intersect); });
}

bool bounding_box::intersect_sphere_cast(const math::double3& start, const math::double3& end, double radius, intersect_data* intersect) const
{
	math::vector3d LocalStart = math::vector3d::transform_coord(math::vector3d(start), get_inverse_transform_matrix());
	math::vector3d LocalEnd = math::vector3d::transform_coord(math::vector3d(end), get_inverse_transform_matrix());

	math::double3 local_start, local_end;
	local_start = LocalStart.to_double3();
	local_end = LocalEnd.to_double3();

	size_t begin = intersect ? intersect->size() : 0;
	inner::axis_aligned_bounding_box aabb(get_local_scale(), math::double3(0.0f, 0.0f, 0.0f));
	bool contact = aabb.intersect_sphere_cast(local_start, local_end, radius, intersect);
	if (contact && intersect)
		intersect->multiply_matrices(begin, get_transeform_matrix());

	return contact;
}

bool bounding_box::intersect_capsule_cast(const math::double3& start, const math::double3& end,
										  double radius, double length, const math::double4& rotation,
										  intersect_data* intersect) const
{
	math::double3 local_start, local_end;
	local_start = (math::vector3d::transform_coord(math::vector3d(start), get_inverse_transform_matrix())).to_double3();
	local_end = (math::vector3d::transform_coord(math::vector3d(end), get_inverse_transform_matrix())).to_double3();

	math::vector3d WorldAxis = (math::quaterniond(rotation).rotate(math::vector3d(0.0f, 1.0f, 0.0f))).normalized();
	math::vector3d LocalAxis = (math::vector3d::transform_normal(WorldAxis, get_inverse_transform_matrix())).normalized();

	size_t begin = intersect ? intersect->size() : 0;
	inner::axis_aligned_bounding_box aabb(get_local_scale(), math::double3(0.0f, 0.0f, 0.0f));
	bool contact = aabb.intersect_capsule_cast_by_axis_spheres(local_start, local_end, radius, length, LocalAxis, intersect);
	if (contact && intersect)
		intersect->multiply_matrices(begin, get_transeform_matrix());
	return contact;
}

void bounding_box::include_corner(math::double3& min, math::double3& max) const
{
	math::double3 vertex[8];
	calculate_vertex(vertex, 8);
	min = vertex[0];
	max = vertex[0];
	for (size_t index = 1; index < 8; ++index)
	{
		min.x	= std::min(vertex[index].x, min.x);
		min.y	= std::min(vertex[index].y, min.y);
		min.z	= std::min(vertex[index].z, min.z);
		max.x	= std::max(vertex[index].x, max.x);
		max.y	= std::max(vertex[index].y, max.y);
		max.z	= std::max(vertex[index].z, max.z);
	}
}

bounding_box bounding_box::multiply_obb(const math::matrixd& matrix) const
{
	//	指定行列の空間へ変換
	math::matrixd LocalMatrix = get_transform_matrix() * matrix.inverted();

	math::vector3d Scale, Position;
	math::quaterniond Rotation;
	LocalMatrix.decompose(Scale, Rotation, Position);
	math::double4 rotation;
	rotation = Rotation.to_double4();
	math::double3 position;
	position = Position.to_double3();
	return bounding_box(get_local_scale(), rotation, position);
}

bounding_box bounding_box::multiply_obb(const math::double4x4& matrix) const
{
	return multiply_obb(math::matrixd(matrix));
}

void bounding_box::calculate_vertex(math::double3* vertices, size_t vertex_count) const
{
	if (!vertices)
		return;

	const math::matrixd RotationMatrix = math::matrixd::rotation_quaternion(math::quaterniond(get_rotation()));
	const math::vector3d AxisX = RotationMatrix.to_vector3(0) * get_local_scale().x;
	const math::vector3d AxisY = RotationMatrix.to_vector3(1) * get_local_scale().y;
	const math::vector3d AxisZ = RotationMatrix.to_vector3(2) * get_local_scale().z;

	math::vector3d Position(get_position());
	math::vector3d Vertices[8];
	math::vector3d PnX = Position - AxisX;
	math::vector3d PpX = Position + AxisX;
	math::vector3d YnZ = AxisY - AxisZ;
	math::vector3d YpZ = AxisY + AxisZ;
	Vertices[0]	= PnX + YnZ;
	Vertices[1]	= PpX + YnZ;
	Vertices[2] = PpX + YpZ;
	Vertices[3]	= PnX + YpZ;

	Vertices[4] = PnX - YpZ;
	Vertices[5] = PpX - YpZ;
	Vertices[6] = PpX - YnZ;
	Vertices[7] = PnX - YnZ;
	for (size_t index = 0; index < vertex_count; ++index)
	{
		vertices[index] = Vertices[index].to_double3();
	}
}
