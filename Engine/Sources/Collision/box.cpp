#include	"collision/box.h"

#include	"collision/sphere.h"
#include	"collision/capsule.h"
#include	"collision/mesh.h"

#include	<DirectXCollision.h>

using namespace collision;

namespace
{
	//	AABBの頂点座標を、u/v（ボロノイ領域を示すビットマスク）から求める
	//	bit 0:x, bit 1:y, bit 2:z　各軸が立っていればmin側、立っていなければmax側の頂点
	static DirectX::XMVECTOR XM_CALLCONV get_aabb_corner(DirectX::XMVECTOR position, DirectX::XMVECTOR extents, int corner_mask)
	{
		DirectX::XMVECTOR Min = DirectX::XMVectorSubtract(position, extents);
		DirectX::XMVECTOR Max = DirectX::XMVectorAdd(position, extents);
		DirectX::XMFLOAT3 min_pos, max_pos;
		DirectX::XMStoreFloat3(&min_pos, Min);
		DirectX::XMStoreFloat3(&max_pos, Max);

		DirectX::XMFLOAT3 corner;
		corner.x = (corner_mask & (1 << 0)) ? min_pos.x : max_pos.x;
		corner.y = (corner_mask & (1 << 1)) ? min_pos.y : max_pos.y;
		corner.z = (corner_mask & (1 << 2)) ? min_pos.z : max_pos.z;
		return DirectX::XMLoadFloat3(&corner);
	}
}

DirectX::XMMATRIX inner::axis_aligned_bounding_box::get_transeform_matrix() const
{
	DirectX::XMVECTOR Scale = DirectX::XMLoadFloat3(&get_local_scale());
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	return	DirectX::XMMatrixScalingFromVector(Scale) * DirectX::XMMatrixTranslationFromVector(Position);
}

DirectX::XMFLOAT4X4 inner::axis_aligned_bounding_box::get_transeform_float4x4() const
{
	DirectX::XMFLOAT4X4 matrix;
	DirectX::XMStoreFloat4x4(&matrix, get_transeform_matrix());
	return matrix;
}

inner::axis_aligned_bounding_box::axis_aligned_bounding_box()
: bounding_object()
{
}

inner::axis_aligned_bounding_box::axis_aligned_bounding_box(const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT3& position)
: bounding_object(scale, DirectX::XMFLOAT4(0, 0, 0, 1), position)
{
}

inner::axis_aligned_bounding_box::~axis_aligned_bounding_box()
{
}

float inner::axis_aligned_bounding_box::calculate_point_to_length(const DirectX::XMFLOAT3& point) const
{
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Scale = DirectX::XMLoadFloat3(&get_local_scale());

	DirectX::XMFLOAT3 min, max;
	DirectX::XMStoreFloat3(&max, DirectX::XMVectorAdd(Position, Scale));
	DirectX::XMStoreFloat3(&min, DirectX::XMVectorSubtract(Position, Scale));
	DirectX::XMFLOAT3 center = point;

	float* center_ptr = &center.x;
	float* min_ptr = &min.x;
	float* max_ptr = &max.x;
#if 01
	float square_length = 0.0f;
	for (size_t index = 0; index < 3; ++index)
	{
		if (center_ptr[index] < min_ptr[index])
		{
			float l = (center_ptr[index] - min_ptr[index]);
			square_length += l * l;
		}
		if (center_ptr[index] > max_ptr[index])
		{
			float l = (center_ptr[index] - max_ptr[index]);
			square_length += l * l;
		}
	}
	square_length = sqrtf(square_length);
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
	DirectX::XMVECTOR Point = DirectX::XMLoadFloat3(&point);
	DirectX::XMVECTOR Center = DirectX::XMLoadFloat3(&center);
	return	DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(Point, Center)));
#endif
}

bool inner::axis_aligned_bounding_box::intersect_point(const DirectX::XMFLOAT3& point) const
{
	return calculate_point_to_length(point) <= FLT_EPSILON;
}

bool inner::axis_aligned_bounding_box::intersect_segment(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, intersect_data* intersect) const
{
	const float* center = &get_position().x;
	const float* extent = &get_local_scale().x;
	const float* s = &start.x;
	const float* e = &end.x;

	//	スラブ法で線分と判定
	float tmin = 0.0f;
	float tmax = 1.0f;
	int hit_axis = -1;
	float hit_sign = 0.0f;
	for (int index = 0; index < 3; ++index)
	{
		float d = e[index] - s[index];
		float min = center[index] - extent[index];
		float max = center[index] + extent[index];
		if (fabsf(d) <= FLT_EPSILON)
		{
			if (s[index] < min || s[index] > max)
				return false;
			continue;
		}

		float ood = 1.0f / d;
		float t1 = (min - s[index]) * ood;
		float t2 = (max - s[index]) * ood;
		float sign = -1.0f;
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
		DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
		DirectX::XMVECTOR Vector = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&end), Start);
		DirectX::XMFLOAT3 position, normal(0.0f, 0.0f, 0.0f);
		DirectX::XMStoreFloat3(&position, DirectX::XMVectorMultiplyAdd(Vector, DirectX::XMVectorReplicate(tmin), Start));
		if (hit_axis >= 0)
			(&normal.x)[hit_axis] = hit_sign;
		else
			DirectX::XMStoreFloat3(&normal, DirectX::XMVectorNegate(Vector));	//	始点が内側
		intersect->add(position, normal, tmin * DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector)));
	}
	return true;
}

bool inner::axis_aligned_bounding_box::intersect_sphere(const DirectX::XMFLOAT3& point, float radius, intersect_data* intersect) const
{
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Scale = DirectX::XMLoadFloat3(&get_local_scale());

	DirectX::XMFLOAT3 min, max;
	DirectX::XMStoreFloat3(&max, DirectX::XMVectorAdd(Position, Scale));
	DirectX::XMStoreFloat3(&min, DirectX::XMVectorSubtract(Position, Scale));

	DirectX::XMFLOAT3 closest_point;
	closest_point.x = math::clamp(point.x, min.x, max.x);
	closest_point.y = math::clamp(point.y, min.y, max.y);
	closest_point.z = math::clamp(point.z, min.z, max.z);

	DirectX::XMVECTOR Point = DirectX::XMLoadFloat3(&point);
	DirectX::XMVECTOR ClosestPoint = DirectX::XMLoadFloat3(&closest_point);
	DirectX::XMVECTOR VectorC_P = DirectX::XMVectorSubtract(ClosestPoint, Point);
	float length_sq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(VectorC_P));
	if (length_sq >= radius * radius)
		return false;

	if (intersect)
	{
		if (length_sq > 1e-12f)
		{
			//	球から箱へ向かう向きに箱を押し出す
			float length = sqrtf(length_sq);
			DirectX::XMFLOAT3 normal;
			DirectX::XMStoreFloat3(&normal, DirectX::XMVectorScale(VectorC_P, 1.0f / length));
			intersect->add(closest_point, normal, radius - length);
		}
		else
		{
			//	球の中心が箱の内側
			DirectX::XMFLOAT3 relative;
			DirectX::XMStoreFloat3(&relative, DirectX::XMVectorSubtract(Point, Position));
			const float* extent = &get_local_scale().x;
			const float* rel = &relative.x;

			//	最も浅い面の軸
			size_t axis_index = 1;
			float best = -1.0f;
			for (size_t index = 0; index < 3; ++index)
			{
				float ratio = (extent[index] > FLT_EPSILON) ? fabsf(rel[index]) / extent[index] : FLT_MAX;
				if (ratio > best)
				{
					best = ratio;
					axis_index = index;
				}
			}

			float sign = rel[axis_index] >= 0.0f ? 1.0f : -1.0f;
			DirectX::XMFLOAT3 normal(0.0f, 0.0f, 0.0f);
			(&normal.x)[axis_index] = -sign;

			DirectX::XMFLOAT3 contact = point;
			(&contact.x)[axis_index] = (&get_position().x)[axis_index] + sign * extent[axis_index];
			intersect->add(contact, normal, radius + extent[axis_index] - fabsf(rel[axis_index]));
		}
	}
	return true;
}

bool inner::axis_aligned_bounding_box::intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect) const
{
	return intersect_sphere(sphere.get_position(), sphere.get_radius(), intersect);
}

bool inner::axis_aligned_bounding_box::intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect) const
{
	DirectX::XMVECTOR Direction = capsule.get_axis_vector();
	DirectX::XMVECTOR d = DirectX::XMVectorScale(Direction, capsule.get_length() * 0.5f);

	DirectX::XMVECTOR PositionA = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR PositionB = DirectX::XMLoadFloat3(&capsule.get_position());
	DirectX::XMVECTOR t = DirectX::XMVectorSubtract(PositionB, PositionA);
	DirectX::XMVECTOR S1 = DirectX::XMVectorSubtract(t, d);
	DirectX::XMVECTOR S2 = DirectX::XMVectorAdd(t, d);
	DirectX::XMVECTOR e = DirectX::XMLoadFloat3(&get_local_scale());

	float pen_depth = FLT_MAX;
	size_t pen_axis = 0;

	//	箱の3軸
	for (size_t index = 0; index < 3; ++index)
	{
		float projR = fabsf(DirectX::XMVectorGetByIndex(d, index)) + capsule.get_radius();
		float pd = (projR + DirectX::XMVectorGetByIndex(e, index)) - fabsf(DirectX::XMVectorGetByIndex(t, index));
		if (pd <= 0.0f)
			return false;
		if (pd < pen_depth)
		{
			pen_depth = pd;
			pen_axis = index;
		}
	}

	DirectX::XMVECTOR capep = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	float adx = DirectX::XMVectorGetX(Direction);
	capep = DirectX::XMVectorSetX(capep, adx < 0.0f ? adx - FLT_EPSILON : adx + FLT_EPSILON);
	float ady = DirectX::XMVectorGetY(Direction);
	capep = DirectX::XMVectorSetY(capep, ady < 0.0f ? ady - FLT_EPSILON : ady + FLT_EPSILON);
	float adz = DirectX::XMVectorGetZ(Direction);
	capep = DirectX::XMVectorSetZ(capep, adz < 0.0f ? adz - FLT_EPSILON : adz + FLT_EPSILON);

	DirectX::XMVECTOR Axes[3] =
	{
		DirectX::XMVectorSet(0.0f, -DirectX::XMVectorGetZ(capep), DirectX::XMVectorGetY(capep), 0.0f),
		DirectX::XMVectorSet(DirectX::XMVectorGetZ(capep), 0.0f, -DirectX::XMVectorGetX(capep), 0.0f),
		DirectX::XMVectorSet(-DirectX::XMVectorGetY(capep), DirectX::XMVectorGetX(capep), 0.0f, 0.0f),
	};

	for (size_t index = 0; index < 3; ++index)
	{
		float mag = DirectX::XMVectorGetX(DirectX::XMVector3Length(Axes[index]));
		float invMag = 1.0f / mag;
		Axes[index] = DirectX::XMVectorScale(Axes[index], invMag);

		float rb = DirectX::XMVectorGetX(e) * fabsf(DirectX::XMVectorGetX(Axes[index]))
					+ DirectX::XMVectorGetY(e) * fabsf(DirectX::XMVectorGetY(Axes[index]))
					+ DirectX::XMVectorGetZ(e) * fabsf(DirectX::XMVectorGetZ(Axes[index]));
		float dist = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(Axes[index], t)));

		float pd = (rb + capsule.get_radius()) - dist;
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
				float offset = (&get_local_scale().x)[clip_axis];

				if (DirectX::XMVectorGetByIndex(S1, clip_axis) < offset && DirectX::XMVectorGetByIndex(S2, clip_axis) > offset)
				{
					DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(S2, S1);
					float w = (offset - DirectX::XMVectorGetByIndex(S1, clip_axis)) / DirectX::XMVectorGetByIndex(Delta, clip_axis);
					S2 = DirectX::XMVectorAdd(S1, DirectX::XMVectorScale(Delta, w));
				}
				else if (DirectX::XMVectorGetByIndex(S1, clip_axis) > offset && DirectX::XMVectorGetByIndex(S2, clip_axis) < offset)
				{
					DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(S1, S2);
					float w = (offset - DirectX::XMVectorGetByIndex(S2, clip_axis)) / DirectX::XMVectorGetByIndex(Delta, clip_axis);
					S1 = DirectX::XMVectorAdd(S2, DirectX::XMVectorScale(Delta, w));
				}

				if (DirectX::XMVectorGetByIndex(S1, clip_axis) > -offset && DirectX::XMVectorGetByIndex(S2, clip_axis) < -offset)
				{
					DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(S2, S1);
					float w = (-offset - DirectX::XMVectorGetByIndex(S1, clip_axis)) / DirectX::XMVectorGetByIndex(Delta, clip_axis);
					S2 = DirectX::XMVectorAdd(S1, DirectX::XMVectorScale(Delta, w));
				}
				else if (DirectX::XMVectorGetByIndex(S1, clip_axis) < -offset && DirectX::XMVectorGetByIndex(S2, clip_axis) > -offset)
				{
					DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(S1, S2);
					float w = (-offset - DirectX::XMVectorGetByIndex(S2, clip_axis)) / DirectX::XMVectorGetByIndex(Delta, clip_axis);
					S1 = DirectX::XMVectorAdd(S2, DirectX::XMVectorScale(Delta, w));
				}
			}

			DirectX::XMVECTOR Normal = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			float element = DirectX::XMVectorGetByIndex(t, pen_axis) > 0.0f ? 1.0f : -1.0f;
			Normal = DirectX::XMVectorSetByIndex(Normal, element, pen_axis);

			//	法線方向に最も深い端点
			float diff = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSubtract(S1, S2), Normal));

			DirectX::XMVECTOR Insert;
			if (fabs(diff) < FLT_EPSILON)
				Insert = DirectX::XMVectorScale(DirectX::XMVectorAdd(S1, S2), 0.5f);
			else if (diff < 0.0f)
				Insert = S1;
			else
				Insert = S2;
			Insert = DirectX::XMVectorSubtract(Insert, DirectX::XMVectorScale(Normal, capsule.get_radius()));
			Insert = DirectX::XMVectorAdd(Insert, DirectX::XMLoadFloat3(&get_position()));

			DirectX::XMFLOAT3 insert, normal;
			DirectX::XMStoreFloat3(&insert, Insert);
			DirectX::XMStoreFloat3(&normal, Normal);
			intersect->add(insert, normal, pen_depth);
		}
		else
		{
			pen_axis -= 3;
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(t, Axes[pen_axis])) < 0.0f)
			{
				Axes[pen_axis] = DirectX::XMVectorNegate(Axes[pen_axis]);
			}

			//	カプセル側へ向いた箱の辺
			DirectX::XMFLOAT3 p1(0.0f, 0.0f, 0.0f), q1(0.0f, 0.0f, 0.0f);
			for (size_t index = 0; index < 3; ++index)
			{
				float extent = DirectX::XMVectorGetByIndex(e, index);
				if (index == pen_axis)
				{
					(&p1.x)[index] = +extent;
					(&q1.x)[index] = -extent;
				}
				else
				{
					float value = DirectX::XMVectorGetByIndex(Axes[pen_axis], index) < 0.0f ? -extent : extent;
					(&p1.x)[index] = value;
					(&q1.x)[index] = value;
				}
			}

			DirectX::XMFLOAT3 s1, s2;
			DirectX::XMStoreFloat3(&s1, S1);
			DirectX::XMStoreFloat3(&s2, S2);
			line line1(line::types::segment, s1, s2);
			line line2(line::types::segment, p1, q1);

			DirectX::XMFLOAT3 cp_aabb, cp_cap;
			float length_aabb, length_cap;
			line1.calculate_distance(cp_cap, length_cap, cp_aabb, length_aabb, line2);

			DirectX::XMVECTOR CP_CAP = DirectX::XMLoadFloat3(&cp_cap);
			DirectX::XMVECTOR P1 = DirectX::XMVectorSubtract(CP_CAP, DirectX::XMVectorScale(Axes[pen_axis], capsule.get_radius()));
			P1 = DirectX::XMVectorAdd(P1, DirectX::XMLoadFloat3(&get_position()));
			DirectX::XMStoreFloat3(&p1, P1);

			DirectX::XMFLOAT3 axes;
			DirectX::XMStoreFloat3(&axes, Axes[pen_axis]);
			intersect->add(p1, axes, pen_depth);
		}

		//	箱を押し出す向きへ反転
		intersect->flip_normals_from(begin);
	}

	return true;
}

bool inner::axis_aligned_bounding_box::intersect_box(const bounding_box& box, intersect_data* intersect) const
{
	//	交差有無のみ
	(void)intersect;
	DirectX::BoundingBox aabb(get_position(), get_local_scale());
	DirectX::BoundingOrientedBox obb(box.get_position(), box.get_local_scale(), box.get_rotation());
	return aabb.Intersects(obb);
}

bool inner::axis_aligned_bounding_box::intersect_box(const axis_aligned_bounding_box& box) const
{
	DirectX::XMVECTOR CenterA = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR ExtentsA = DirectX::XMLoadFloat3(&get_local_scale());

	DirectX::XMVECTOR CenterB = DirectX::XMLoadFloat3(&box.get_position());
	DirectX::XMVECTOR ExtentsB = DirectX::XMLoadFloat3(&box.get_local_scale());

	DirectX::XMVECTOR MinA = DirectX::XMVectorSubtract(CenterA, ExtentsA);
	DirectX::XMVECTOR MaxA = DirectX::XMVectorAdd(CenterA, ExtentsA);

	DirectX::XMVECTOR MinB = DirectX::XMVectorSubtract(CenterB, ExtentsB);
	DirectX::XMVECTOR MaxB = DirectX::XMVectorAdd(CenterB, ExtentsB);

	DirectX::XMVECTOR Disjoint = DirectX::XMVectorOrInt(DirectX::XMVectorGreater(MinA, MaxB),
														DirectX::XMVectorGreater(MinB, MaxA));
	return !DirectX::Internal::XMVector3AnyTrue(Disjoint);
}

bool inner::axis_aligned_bounding_box::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	//	未対応
	return false;
}

bool inner::axis_aligned_bounding_box::intersect_sphere_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float radius, intersect_data* intersect) const
{
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
	DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);

	return intersect_sphere_cast(Start, End, radius, intersect);
}

bool inner::axis_aligned_bounding_box::intersect_sphere_cast(DirectX::XMVECTOR Start, DirectX::XMVECTOR End, float radius, intersect_data* intersect) const
{
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Extents = DirectX::XMLoadFloat3(&get_local_scale());

	//	スフィアキャストの始端、AABBの中心、AABBとスフィアキャスト半径のボロノイ領域の半径を
	//	ループ処理するため、xyz成分を配列に代入する
	float start_pos_array[3] = { DirectX::XMVectorGetX(Start), DirectX::XMVectorGetY(Start), DirectX::XMVectorGetZ(Start) };
	float aabb_pos_array[3] = { DirectX::XMVectorGetX(Position), DirectX::XMVectorGetY(Position), DirectX::XMVectorGetZ(Position) };
	float aabb_extent_array[3] = { DirectX::XMVectorGetX(Extents), DirectX::XMVectorGetY(Extents), DirectX::XMVectorGetZ(Extents) };
	float aabb_voronoi_rad_array[3] =
	{
		aabb_extent_array[0] + radius,
		aabb_extent_array[1] + radius,
		aabb_extent_array[2] + radius
	};

	//	スフィアキャストのベクトルを作り、距離を変数にバックアップしたうえで正規化し、
	//	こちらもループ処理するため、xyz成分を配列に代入
	DirectX::XMVECTOR DVec = DirectX::XMVectorSubtract(End, Start);
	float ray_length = DirectX::XMVectorGetX(DirectX::XMVector3Length(DVec));
	if (ray_length <= 1e-6f)
		return false;
	DVec = DirectX::XMVector3Normalize(DVec);
	float d_array[3] = { DirectX::XMVectorGetX(DVec), DirectX::XMVectorGetY(DVec), DirectX::XMVectorGetZ(DVec) };

	//	開始時の重なり
	{
		DirectX::XMFLOAT3 closest;
		float* closest_array = &closest.x;
		for (int i = 0; i < 3; ++i)
			closest_array[i] = math::clamp(start_pos_array[i], aabb_pos_array[i] - aabb_extent_array[i], aabb_pos_array[i] + aabb_extent_array[i]);
		DirectX::XMVECTOR Closest = DirectX::XMLoadFloat3(&closest);
		DirectX::XMVECTOR Vector = DirectX::XMVectorSubtract(Start, Closest);
		float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
		if (distance < radius)
		{
			DirectX::XMVECTOR Normal;
			if (distance > 1e-6f)
			{
				Normal = DirectX::XMVectorScale(Vector, 1.0f / distance);
			}
			else
			{
				//	中心が箱の内側なら最も浅い面
				size_t axis_index = 1;
				float best = -1.0f;
				for (size_t i = 0; i < 3; ++i)
				{
					float relative = start_pos_array[i] - aabb_pos_array[i];
					float ratio = (aabb_extent_array[i] > FLT_EPSILON) ? fabsf(relative) / aabb_extent_array[i] : FLT_MAX;
					if (ratio > best)
					{
						best = ratio;
						axis_index = i;
					}
				}
				DirectX::XMFLOAT3 normal(0.0f, 0.0f, 0.0f);
				(&normal.x)[axis_index] = (start_pos_array[axis_index] - aabb_pos_array[axis_index]) >= 0.0f ? 1.0f : -1.0f;
				Normal = DirectX::XMLoadFloat3(&normal);
			}
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(DVec, Normal)) >= 0.0f)
				return false;
			add_cast_hit(intersect, Start, Closest, Normal, 0.0f);
			return true;
		}
	}

	//	直線とスラブの２交点までの距離をtminとtmaxと定義
	float tmin = 0.0f;
	float tmax = FLT_MAX;

	//	スラブとの距離を算出し交差しているかの確認と最近点の算出を行う
	for (int i = 0; i < 3; i++)
	{
		//	xyz軸との平行確認
		if (fabsf(d_array[i]) < FLT_EPSILON)
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
			float ood = 1.0f / d_array[i];
			float t1 = (aabb_pos_array[i] - aabb_voronoi_rad_array[i] - start_pos_array[i]) * ood;
			float t2 = (aabb_pos_array[i] + aabb_voronoi_rad_array[i] - start_pos_array[i]) * ood;

			//	遠近が逆転している場合があるので、その場合入れ替えておく
			if (t1 > t2)
			{
				float tmp = t1;
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
	DirectX::XMFLOAT3 point =
	{
		start_pos_array[0] + d_array[0] * tmin,
		start_pos_array[1] + d_array[1] * tmin,
		start_pos_array[2] + d_array[2] * tmin,
	};

	//	求めた交点から、頂点・面・辺のボロノイ領域のチェックを行う
	//	u をマイナス、v をプラスとし、ビットでxyzの向きを管理している
	int u = 0, v = 0;
	if (point.x <= aabb_pos_array[0] - DirectX::XMVectorGetX(Extents)) u |= (1 << 0);
	else if (point.x >= aabb_pos_array[0] + DirectX::XMVectorGetX(Extents)) v |= (1 << 0);
	if (point.y <= aabb_pos_array[1] - DirectX::XMVectorGetY(Extents)) u |= (1 << 1);
	else if (point.y >= aabb_pos_array[1] + DirectX::XMVectorGetY(Extents)) v |= (1 << 1);
	if (point.z <= aabb_pos_array[2] - DirectX::XMVectorGetZ(Extents)) u |= (1 << 2);
	else if (point.z >= aabb_pos_array[2] + DirectX::XMVectorGetZ(Extents)) v |= (1 << 2);

	int mask = u | v;

	if (mask != 0 && (mask & (mask - 1)) == 0)	//	ボロノイ面領域の場合
	{
		//	面の外向き法線
		DirectX::XMVECTOR Normal = DirectX::XMVectorSet(
			(float)(((v & 1) - (u & 1)) >> 0),
			(float)(((v & 2) - (u & 2)) >> 1),
			(float)(((v & 4) - (u & 4)) >> 2),
			0.0f);

		DirectX::XMVECTOR Center = DirectX::XMLoadFloat3(&point);
		DirectX::XMVECTOR Contact = DirectX::XMVectorSubtract(Center, DirectX::XMVectorScale(Normal, radius));
		add_cast_hit(intersect, Center, Contact, Normal, tmin);
		return true;
	}

	//	辺・頂点領域は全頂点の球と全辺の円柱で最も近い交点を求める
	DirectX::XMVECTOR NearPos = {};
	float near_dist = FLT_MAX;

	DirectX::XMVECTOR HitPosition;
	float hit_distance;
	for (int corner = 0; corner < 8; ++corner)
	{
		DirectX::XMVECTOR Vertex = get_aabb_corner(Position, Extents, corner);
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

			DirectX::XMVECTOR StartVertex = get_aabb_corner(Position, Extents, corner);
			DirectX::XMVECTOR EndVertex = get_aabb_corner(Position, Extents, corner | bit);
			if (collision::intersect_ray_vs_cylinder(Start, End, StartVertex, EndVertex, radius, HitPosition, hit_distance) && hit_distance > 0.0f && hit_distance < near_dist)
			{
				DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(HitPosition, StartVertex);
				DirectX::XMVECTOR EdgeVec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(EndVertex, StartVertex));
				NearPos = DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Dot(Vec, EdgeVec), EdgeVec, StartVertex);
				near_dist = hit_distance;
			}
		}
	}

	if (near_dist == FLT_MAX)
		return false;

	//	接触点（AABB側の頂点/辺上の最近点）から、実際に球が接触した位置へ法線を求める
	DirectX::XMVECTOR Center = DirectX::XMVectorMultiplyAdd(DVec, DirectX::XMVectorReplicate(near_dist), Start);
	DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(Center, NearPos);
	float delta_length = DirectX::XMVectorGetX(DirectX::XMVector3Length(Delta));
	DirectX::XMVECTOR Normal = (delta_length > 1e-6f) ? DirectX::XMVectorScale(Delta, 1.0f / delta_length) : DirectX::XMVectorNegate(DVec);
	add_cast_hit(intersect, Center, NearPos, Normal, near_dist);
	return true;
}

bool inner::axis_aligned_bounding_box::intersect_capsule_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
															   float radius, float length, const DirectX::XMFLOAT4& rotation,
															   intersect_data* intersect) const
{
	DirectX::XMVECTOR axis = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XMLoadFloat4(&rotation));
	return intersect_capsule_cast_by_axis_spheres(start, end, radius, length, axis, intersect);
}

void inner::axis_aligned_bounding_box::include_corner(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max) const
{
	DirectX::XMVECTOR Min, Max;
	include_corner(Min, Max);
	DirectX::XMStoreFloat3(&min, Min);
	DirectX::XMStoreFloat3(&max, Max);
}

void inner::axis_aligned_bounding_box::include_corner(DirectX::XMVECTOR& Min, DirectX::XMVECTOR& Max) const
{
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Scale = DirectX::XMLoadFloat3(&get_local_scale());
	Min = DirectX::XMVectorSubtract(Position, Scale);
	Max = DirectX::XMVectorAdd(Position, Scale);
}

void inner::axis_aligned_bounding_box::calculate_vertex(DirectX::XMFLOAT3* vertices, size_t vertex_count) const
{
	if (!vertices)
		return;

	DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR X = DirectX::XMVectorSet(get_local_scale().x, 0.0f, 0.0f, 0.0f);
	DirectX::XMVECTOR Y = DirectX::XMVectorSet(0.0f, get_local_scale().y, 0.0f, 0.0f);
	DirectX::XMVECTOR Z = DirectX::XMVectorSet(0.0f, 0.0f, get_local_scale().z, 0.0f);
	DirectX::XMVECTOR AddPpX = DirectX::XMVectorAdd(P, X);
	DirectX::XMVECTOR AddPnX = DirectX::XMVectorSubtract(P, X);
	DirectX::XMVECTOR AddYpZ = DirectX::XMVectorAdd(Y, Z);
	DirectX::XMVECTOR AddYnZ = DirectX::XMVectorSubtract(Y, Z);
	DirectX::XMStoreFloat3(&vertices[0], DirectX::XMVectorAdd(AddPnX, AddYnZ));
	DirectX::XMStoreFloat3(&vertices[1], DirectX::XMVectorAdd(AddPpX, AddYnZ));
	DirectX::XMStoreFloat3(&vertices[2], DirectX::XMVectorAdd(AddPnX, AddYpZ));
	DirectX::XMStoreFloat3(&vertices[3], DirectX::XMVectorAdd(AddPpX, AddYpZ));

	DirectX::XMStoreFloat3(&vertices[4], DirectX::XMVectorSubtract(AddPnX, AddYnZ));
	DirectX::XMStoreFloat3(&vertices[5], DirectX::XMVectorSubtract(AddPpX, AddYnZ));
	DirectX::XMStoreFloat3(&vertices[6], DirectX::XMVectorSubtract(AddPnX, AddYpZ));
	DirectX::XMStoreFloat3(&vertices[7], DirectX::XMVectorSubtract(AddPpX, AddYpZ));
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

	void	clipPolygonToPlaneNegative(const DirectX::XMVECTOR& planeN, float planeOffset,
									   const DirectX::XMVECTOR* verts, size_t vertCount,
									   DirectX::XMVECTOR* out, size_t& outCount)
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
				float offset = DirectX::XMVectorGetX(DirectX::XMVector3Dot(verts[0], planeN)) - planeOffset;
				if (offset <= FLT_EPSILON)
				{
					out[0] = verts[0];
					outCount = 1;
				}
				break;
			}
			case 2:
			{
				const DirectX::XMVECTOR& start = verts[0];
				const DirectX::XMVECTOR& end = verts[1];
			
				float startOffset = DirectX::XMVectorGetX(DirectX::XMVector3Dot(start, planeN)) - planeOffset;
				float endOffset = DirectX::XMVectorGetX(DirectX::XMVector3Dot(end, planeN)) - planeOffset;

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
						DirectX::XMVECTOR edge = DirectX::XMVectorSubtract(end, start);
						float d = -startOffset / DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge, planeN));

						out[0] = start;
						out[1] = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(edge, d));
						outCount = 2;
						break;				
					}
					case (START_OUT | END_IN):
					{
						DirectX::XMVECTOR edge = DirectX::XMVectorSubtract(end, start);
						float d = -startOffset / DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge, planeN));

						out[0] = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(edge, d));
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
				DirectX::XMVECTOR start = verts[vertCount-1];

				for (size_t index = 0; index < vertCount; ++index)
				{
					const DirectX::XMVECTOR& end = verts[index];

					float startOffset = DirectX::XMVectorGetX(DirectX::XMVector3Dot(start, planeN)) - planeOffset;	
					float endOffset = DirectX::XMVectorGetX(DirectX::XMVector3Dot(end, planeN)) - planeOffset;

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
							DirectX::XMVECTOR edge = DirectX::XMVectorSubtract(end, start);
							float d = -startOffset / DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge, planeN));

							out[outCount] = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(edge, d));
							++outCount;					
						}
						break;
						case (START_OUT | END_IN):
						{
							DirectX::XMVECTOR edge = DirectX::XMVectorSubtract(end, start);
							float d = -startOffset / DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge, planeN));

							out[outCount] = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(edge, d));
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

	bool face_contact_obb_to_aabb(const bounding_box& obb, const inner::axis_aligned_bounding_box& aabb, size_t axis_index, DirectX::XMVECTOR& InsertPoint)
	{
		DirectX::XMMATRIX ObbMatrix = obb.get_transeform_matrix();
		DirectX::XMVECTOR Axis = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(ObbMatrix.r[3], DirectX::XMLoadFloat3(&aabb.get_position()));
	
		Axis = DirectX::XMVectorSetByIndex(Axis, DirectX::XMVectorGetByIndex(Delta, axis_index) < 0.0f ? -1.0f : 1.0f, axis_index);

		DirectX::XMVECTOR LocalAxis = DirectX::XMVector3TransformNormal(Axis, DirectX::XMMatrixTranspose(ObbMatrix));

		DirectX::XMVECTOR ScaledAxis = DirectX::XMVectorDivide(DirectX::XMVectorNegate(LocalAxis), DirectX::XMLoadFloat3(&obb.get_local_scale()));

		size_t face_axis = 0;
		if (fabs(DirectX::XMVectorGetX(ScaledAxis)) < fabs(DirectX::XMVectorGetY(ScaledAxis)))
		{
			face_axis = 1;
		}
		if (fabs(DirectX::XMVectorGetByIndex(ScaledAxis, face_axis)) < fabs(DirectX::XMVectorGetZ(ScaledAxis)))
		{
			face_axis = 2;
		}

		DirectX::XMVECTOR Vertices[12];
		size_t face_index = DirectX::XMVectorGetByIndex(ScaledAxis, face_axis) < 0.0f ? face_axis : face_axis + 3;

		static const DirectX::XMVECTOR UnitVertices[8] =
		{
			DirectX::XMVectorSet(-1.0f, -1.0f, -1.0f, 0.0f),
			DirectX::XMVectorSet(-1.0f, -1.0f,  1.0f, 0.0f),
			DirectX::XMVectorSet(-1.0f,  1.0f, -1.0f, 0.0f),
			DirectX::XMVectorSet(-1.0f,  1.0f,  1.0f, 0.0f),
			DirectX::XMVectorSet( 1.0f, -1.0f, -1.0f, 0.0f),
			DirectX::XMVectorSet( 1.0f, -1.0f,  1.0f, 0.0f),
			DirectX::XMVectorSet( 1.0f,  1.0f, -1.0f, 0.0f),
			DirectX::XMVectorSet( 1.0f,  1.0f,  1.0f, 0.0f)
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
			Vertices[index] = DirectX::XMVectorMultiply(UnitVertices[UnitPlaneFace[face_index * 4 + index]], DirectX::XMLoadFloat3(&obb.get_local_scale()));

			Vertices[index] = DirectX::XMVector3TransformCoord(Vertices[index], ObbMatrix);
		}

		size_t clip_axes[][2]	=
		{
			{ 1, 2 },
			{ 0, 2 },
			{ 0, 1 }
		};

		DirectX::XMVECTOR Buffer[12];
		DirectX::XMVECTOR* Input = Vertices;
		DirectX::XMVECTOR* Output = Buffer;
		size_t input_count = 4;
		size_t output_count = 0;
		for(int a = 0; a < 2; ++a)
		{
			size_t index = clip_axes[axis_index][a];

			DirectX::XMVECTOR PlaneN = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			PlaneN = DirectX::XMVectorSetByIndex(PlaneN, 1.0f, index);
			float planeOffset = (&aabb.get_local_scale().x)[index];
			
			clipPolygonToPlaneNegative(PlaneN, planeOffset, Input, input_count, Output, output_count);

			input_count = output_count;
			output_count = 0;
			DirectX::XMVECTOR* Temp = Input;
			Input = Output;
			Output = Temp;

			PlaneN = DirectX::XMVectorSetByIndex(PlaneN, -1.0f, index);
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
		float distances[12];
		for (size_t index = 0; index < size; ++index)
		{
			distances[index] = DirectX::XMVectorGetByIndex(Input[index], axis_index) * DirectX::XMVectorGetByIndex(Axis, axis_index);
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

		DirectX::XMVECTOR Average = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		size_t count = 0;
		for (; count < input_count; ++count)
		{
			if (fabsf(distances[0] - distances[count]) > 0.01f)
				break;

			Average = DirectX::XMVectorAdd(Average, Input[count]);
		}

		Average = DirectX::XMVectorDivide(Average, DirectX::XMVectorSet(static_cast<float>(count), static_cast<float>(count), static_cast<float>(count), static_cast<float>(count)));
		InsertPoint = DirectX::XMVectorAdd(Average, DirectX::XMLoadFloat3(&aabb.get_position()));
		return true;
	}
}	//	namespace

////////////////////////////////////////////////////////////////////////////////////////////
DirectX::XMMATRIX bounding_box::get_transeform_matrix() const
{
	return	get_transform_matrix();
}

DirectX::XMFLOAT4X4 bounding_box::get_transeform_float4x4() const
{
	DirectX::XMFLOAT4X4 matrix;
	DirectX::XMStoreFloat4x4(&matrix, get_transeform_matrix());
	return matrix;
}

bounding_box::bounding_box()
: bounding_object()
{
}

bounding_box::bounding_box(const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& rotation, const DirectX::XMFLOAT3& position)
: bounding_object(scale, rotation, position)
{
}

bounding_box::bounding_box(const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT3& y_axis, float angle, const DirectX::XMFLOAT3& position)
: bounding_object(scale, DirectX::XMFLOAT4(0, 0, 0, 1), position)
{
	DirectX::XMVECTOR YAxis = DirectX::XMLoadFloat3(&y_axis);
	DirectX::XMVECTOR Rotation = DirectX::XMQuaternionRotationAxis(YAxis, angle);
	DirectX::XMFLOAT4 rotation;
	DirectX::XMStoreFloat4(&rotation, Rotation);
	set_rotation(rotation);
}

bounding_box::~bounding_box()
{
}

float bounding_box::calculate_point_to_length(const DirectX::XMFLOAT3& point) const
{
	DirectX::XMVECTOR Vector = DirectX::XMVectorSet(0, 0, 0, 0);   // 最終的に長さを求めるベクトル

	DirectX::XMVECTOR Point = DirectX::XMLoadFloat3(&point);
	DirectX::XMVECTOR PositionObb = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&get_rotation()));

	//	各軸についてはみ出た部分のベクトルを算出
	const float* length = &get_local_scale().x;
	for (int axis = 0; axis < 3; axis++)
	{
		if (length[axis] <= 0)
			continue;	//	L = 0は計算できない

		DirectX::XMVECTOR PointToPositionObb = DirectX::XMVectorSubtract(Point, PositionObb);
		DirectX::XMVECTOR ObbAxis = Rotation.r[axis];
		float s = DirectX::XMVectorGetX(DirectX::XMVector3Dot(PointToPositionObb, ObbAxis)) / length[axis];

		//	sの値から、はみ出した部分があればそのベクトルを加算
		s = fabsf(s);
		if (s > 1)
		{
			s = (1 - s) * length[axis];
			//	はみ出した部分のベクトル算出
			Vector = DirectX::XMVectorAdd(Vector,
									DirectX::XMVectorMultiply(ObbAxis, DirectX::XMVectorSet(s, s, s, s)));
		}
	}

	//	長さを出力
	return	DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
}

bool bounding_box::intersect_point(const DirectX::XMFLOAT3& point) const
{
	return calculate_point_to_length(point) <= FLT_EPSILON;
}

bool bounding_box::intersect_segment(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, intersect_data* intersect) const
{
	//	ローカル空間のAABBで判定
	DirectX::XMFLOAT3 local_start, local_end;
	DirectX::XMStoreFloat3(&local_start, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&start), get_inverse_transform_matrix()));
	DirectX::XMStoreFloat3(&local_end, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&end), get_inverse_transform_matrix()));

	size_t begin = intersect ? intersect->size() : 0;
	inner::axis_aligned_bounding_box aabb(get_local_scale(), {0.0f, 0.0f, 0.0f});
	bool contact = aabb.intersect_segment(local_start, local_end, intersect);
	if (contact && intersect)
		intersect->multiply_matrices(begin, get_transeform_matrix());
	return contact;
}

bool bounding_box::intersect_sphere(const DirectX::XMFLOAT3& point, float radius, intersect_data* intersect) const
{
	DirectX::XMVECTOR LocalPoint;
	LocalPoint = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point), get_inverse_transform_matrix());

	DirectX::XMFLOAT3 new_point;
	DirectX::XMStoreFloat3(&new_point, LocalPoint);

	size_t begin = intersect ? intersect->size() : 0;
	inner::axis_aligned_bounding_box aabb(get_local_scale(), {0.0f, 0.0f, 0.0f});
	bool contact = aabb.intersect_sphere(new_point, radius, intersect);
	if (contact && intersect)
		intersect->multiply_matrices(begin, get_transeform_matrix());
	return contact;
}

bool bounding_box::intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect) const
{
	return	intersect_sphere(sphere.get_position(), sphere.get_radius(), intersect);
}

bool bounding_box::intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect) const
{
	DirectX::XMMATRIX InverseCapsuleRotationMatrix;
	InverseCapsuleRotationMatrix = DirectX::XMMatrixMultiply(capsule.get_transform_matrix(), get_inverse_transform_matrix());

	DirectX::XMVECTOR Dummy, LocalCapsuleRotation, LocalCapsulePosition;
	DirectX::XMMatrixDecompose(&Dummy , &LocalCapsuleRotation, &LocalCapsulePosition, InverseCapsuleRotationMatrix);

	DirectX::XMFLOAT3 local_capsule_position;
	DirectX::XMStoreFloat3(&local_capsule_position, LocalCapsulePosition);
	DirectX::XMFLOAT4 local_rotation;
	DirectX::XMStoreFloat4(&local_rotation, LocalCapsuleRotation);
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
	DirectX::XMMATRIX xformA = get_transeform_matrix();
	DirectX::XMVECTOR extentsA = DirectX::XMLoadFloat3(&get_local_scale());

	DirectX::XMMATRIX xformB = box.get_transeform_matrix();
	DirectX::XMVECTOR extentsB = DirectX::XMLoadFloat3(&box.get_local_scale());

	DirectX::XMFLOAT3X3 r;
	r.m[0][0] = DirectX::XMVectorGetX(DirectX::XMVector3Dot(xformA.r[0], xformB.r[0]));
	r.m[0][1] = DirectX::XMVectorGetX(DirectX::XMVector3Dot(xformA.r[0], xformB.r[1]));
	r.m[0][2] = DirectX::XMVectorGetX(DirectX::XMVector3Dot(xformA.r[0], xformB.r[2]));

	r.m[1][0] = DirectX::XMVectorGetX(DirectX::XMVector3Dot(xformA.r[1], xformB.r[0]));
	r.m[1][1] = DirectX::XMVectorGetX(DirectX::XMVector3Dot(xformA.r[1], xformB.r[1]));
	r.m[1][2] = DirectX::XMVectorGetX(DirectX::XMVector3Dot(xformA.r[1], xformB.r[2]));

	r.m[2][0] = DirectX::XMVectorGetX(DirectX::XMVector3Dot(xformA.r[2], xformB.r[0]));
	r.m[2][1] = DirectX::XMVectorGetX(DirectX::XMVector3Dot(xformA.r[2], xformB.r[1]));
	r.m[2][2] = DirectX::XMVectorGetX(DirectX::XMVector3Dot(xformA.r[2], xformB.r[2]));

	DirectX::XMVECTOR t = DirectX::XMVectorSubtract(xformB.r[3], xformA.r[3]);
	t = DirectX::XMVectorSet(DirectX::XMVectorGetX(DirectX::XMVector3Dot(t, xformA.r[0])),
							 DirectX::XMVectorGetX(DirectX::XMVector3Dot(t, xformA.r[1])),
							 DirectX::XMVectorGetX(DirectX::XMVector3Dot(t, xformA.r[2])),
							 0.0f);

	DirectX::XMFLOAT3X3 abs_r;
	for(int i = 0; i < 3; ++i)
	{
		for(int j = 0; j < 3; ++j)
		{
			abs_r.m[i][j] = abs( r.m[i][j] ) + FLT_EPSILON;
		}
	}

	float penetration_depth = FLT_MAX;
	DirectX::XMFLOAT3 axis(0.0f, 0.0f, 0.0f);
	size_t world_axis = 0;
	for (size_t index = 0; index < 3; ++index)
	{
		float	ra	= DirectX::XMVectorGetByIndex(extentsA, index);
		float	rb	= DirectX::XMVectorGetByIndex(extentsB, 0) * abs_r.m[index][0]
					+ DirectX::XMVectorGetByIndex(extentsB, 1) * abs_r.m[index][1]
					+ DirectX::XMVectorGetByIndex(extentsB, 2) * abs_r.m[index][2];

		float d = fabsf(DirectX::XMVectorGetByIndex(t, index));

		float pd = (ra + rb) - d;
			
		if (pd <= 0.0f)
			return false;

		if (pd < penetration_depth)
		{
			penetration_depth = pd;
			axis = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			(&axis.x)[index] = 1.0f;
			world_axis = index;
		}
	}

	for (size_t index = 0; index < 3; ++index)
	{
		float ra = DirectX::XMVectorGetX(extentsA) * abs_r.m[0][index]
				 + DirectX::XMVectorGetY(extentsA) * abs_r.m[1][index]
				 + DirectX::XMVectorGetZ(extentsA) * abs_r.m[2][index];
		float rb = DirectX::XMVectorGetByIndex(extentsB, index);

		float d = fabsf(DirectX::XMVectorGetX(t) * r.m[0][index] + DirectX::XMVectorGetY(t) * r.m[1][index] + DirectX::XMVectorGetZ(t) * r.m[2][index]);
		float pd = (ra + rb) - d;

		if (pd <= 0.0f)
			return false;

		if (pd < penetration_depth)
		{
			penetration_depth = pd;
			axis = DirectX::XMFLOAT3(r.m[0][index], r.m[1][index], r.m[2][index]);
			world_axis = index + 3;
		}
	}

	DirectX::XMFLOAT3 axes[9];
	float pds[9] = {0.0f};

	for (size_t indexA = 0; indexA < 3; ++indexA)
	{
		DirectX::XMFLOAT3 axisA(0.0f, 0.0f, 0.0f);
		(&axisA.x)[indexA] = 1.0f;

		for (size_t indexB = 0; indexB < 3; ++indexB)
		{
			DirectX::XMFLOAT3 axisB(r.m[0][indexB], r.m[1][indexB], r.m[2][indexB]);

			axisB.x += (axisB.x < 0.0f) ? -FLT_EPSILON : FLT_EPSILON;
			axisB.y += (axisB.y < 0.0f) ? -FLT_EPSILON : FLT_EPSILON;
			axisB.z += (axisB.z < 0.0f) ? -FLT_EPSILON : FLT_EPSILON;

			DirectX::XMFLOAT3& current_axis = axes[indexA * 3 + indexB];
			DirectX::XMVECTOR AxisA = DirectX::XMLoadFloat3(&axisA);
			DirectX::XMVECTOR AxisB = DirectX::XMLoadFloat3(&axisB);
			DirectX::XMStoreFloat3(&current_axis, DirectX::XMVector3Cross(AxisA, AxisB));

			float ra = DirectX::XMVectorGetX(extentsA) * fabsf(current_axis.x)
					 + DirectX::XMVectorGetY(extentsA) * fabsf(current_axis.y)
					 + DirectX::XMVectorGetZ(extentsA) * fabsf(current_axis.z);

			float rb = DirectX::XMVectorGetX(extentsB) * fabsf(current_axis.x * r.m[0][0] + current_axis.y * r.m[1][0] + current_axis.z * r.m[2][0])
					 + DirectX::XMVectorGetY(extentsB) * fabsf(current_axis.x * r.m[0][1] + current_axis.y * r.m[1][1] + current_axis.z * r.m[2][1])
					 + DirectX::XMVectorGetZ(extentsB) * fabsf(current_axis.x * r.m[0][2] + current_axis.y * r.m[1][2] + current_axis.z * r.m[2][2]);

			float& pd = pds[indexA * 3 + indexB];

			pd = (ra + rb) - fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(t, DirectX::XMLoadFloat3(&current_axis))));

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

				DirectX::XMFLOAT3& current_axis = axes[index];
				float& pd = pds[index];

				float mag = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&current_axis)));

				if (mag < FLT_EPSILON)
					continue;

				float inv_mag = 1.0f / mag;
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

		if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&axis), t)) > 0.0f)
		{
			DirectX::XMStoreFloat3(&axis, DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&axis)));
		}
			
		//	ワールド法線
		DirectX::XMVECTOR NormalWorld = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&axis), xformA));

		//	指定方向に最も突き出た頂点
		auto support_point = [](const bounding_box& target, DirectX::XMVECTOR Direction)
		{
			DirectX::XMFLOAT3 vertices[8];
			target.calculate_vertex(vertices, 8);
			DirectX::XMVECTOR Result = DirectX::XMLoadFloat3(&vertices[0]);
			float best = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Result, Direction));
			for (size_t index = 1; index < 8; ++index)
			{
				DirectX::XMVECTOR Vertex = DirectX::XMLoadFloat3(&vertices[index]);
				float value = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Vertex, Direction));
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
			auto edge_points = [](DirectX::XMVECTOR Direction, const DirectX::XMFLOAT3& extent, size_t edge_axis, DirectX::XMVECTOR& P, DirectX::XMVECTOR& Q)
			{
				DirectX::XMFLOAT3 p(0.0f, 0.0f, 0.0f), q(0.0f, 0.0f, 0.0f);
				for (size_t index = 0; index < 3; ++index)
				{
					float value = (&extent.x)[index];
					if (index == edge_axis)
					{
						(&p.x)[index] = +value;
						(&q.x)[index] = -value;
					}
					else
					{
						value = DirectX::XMVectorGetByIndex(Direction, index) < 0.0f ? -value : value;
						(&p.x)[index] = value;
						(&q.x)[index] = value;
					}
				}
				P = DirectX::XMLoadFloat3(&p);
				Q = DirectX::XMLoadFloat3(&q);
			};

			//	AはB側(-axis)、BはA側(+axis)の辺
			DirectX::XMVECTOR PA, QA, PB, QB;
			edge_points(DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&axis)), get_local_scale(), axisA, PA, QA);
			DirectX::XMVECTOR AxisLocalB = DirectX::XMVector3TransformNormal(NormalWorld, DirectX::XMMatrixTranspose(xformB));
			edge_points(AxisLocalB, box.get_local_scale(), axisB, PB, QB);

			//	Bのローカル→Aのローカル
			DirectX::XMMATRIX InverseA = get_inverse_transform_matrix();
			PB = DirectX::XMVector3TransformCoord(DirectX::XMVector3TransformCoord(PB, xformB), InverseA);
			QB = DirectX::XMVector3TransformCoord(DirectX::XMVector3TransformCoord(QB, xformB), InverseA);

			DirectX::XMFLOAT3 pA, pB, qA, qB;
			DirectX::XMStoreFloat3(&pA, PA);
			DirectX::XMStoreFloat3(&pB, PB);
			DirectX::XMStoreFloat3(&qA, QA);
			DirectX::XMStoreFloat3(&qB, QB);

			line line1(line::types::segment, pA, qA);
			line line2(line::types::segment, pB, qB);

			DirectX::XMFLOAT3 cpA, cpB;
			float deltaA, deltaB;
			line1.calculate_distance(cpA, deltaA, cpB, deltaB, line2);

			//	両辺の最近点の中点
			DirectX::XMVECTOR Contact = DirectX::XMVectorScale(DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&cpA), DirectX::XMLoadFloat3(&cpB)), 0.5f);
			DirectX::XMFLOAT3 contact, normal;
			DirectX::XMStoreFloat3(&contact, DirectX::XMVector3TransformCoord(Contact, xformA));
			DirectX::XMStoreFloat3(&normal, NormalWorld);
			intersect->add(contact, normal, penetration_depth);
		}
		else	//	線と面 / 面と面
		{
			DirectX::XMVECTOR ContactPt;
			if (world_axis < 3)
			{
				//	Aの面にめり込んだBの頂点
				bounding_box local_obb = box.multiply_obb(get_transeform_matrix());
				inner::axis_aligned_bounding_box local_aabb(get_local_scale(), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

				if (face_contact_obb_to_aabb(local_obb, local_aabb, world_axis, ContactPt))
					ContactPt = DirectX::XMVector3TransformCoord(ContactPt, get_transeform_matrix());
				else
					ContactPt = support_point(box, NormalWorld);
			}
			else
			{
				//	Bの面にめり込んだAの頂点
				bounding_box local_obb = multiply_obb(box.get_transeform_matrix());
				inner::axis_aligned_bounding_box local_aabb(box.get_local_scale(), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

				if (face_contact_obb_to_aabb(local_obb, local_aabb, world_axis - 3, ContactPt))
					ContactPt = DirectX::XMVector3TransformCoord(ContactPt, box.get_transeform_matrix());
				else
					ContactPt = support_point(*this, DirectX::XMVectorNegate(NormalWorld));
			}

			DirectX::XMFLOAT3 contactPt, normal;
			DirectX::XMStoreFloat3(&contactPt, ContactPt);
			DirectX::XMStoreFloat3(&normal, NormalWorld);
			intersect->add(contactPt, normal, penetration_depth);
		}
	}
	return true;
}

bool bounding_box::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	return delegate_flipped(intersect, [&]() { return mesh.intersect_box(*this, intersect); });
}

bool bounding_box::intersect_sphere_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float radius, intersect_data* intersect) const
{
	DirectX::XMVECTOR LocalStart = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&start), get_inverse_transform_matrix());
	DirectX::XMVECTOR LocalEnd = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&end), get_inverse_transform_matrix());

	DirectX::XMFLOAT3 local_start, local_end;
	DirectX::XMStoreFloat3(&local_start, LocalStart);
	DirectX::XMStoreFloat3(&local_end, LocalEnd);

	size_t begin = intersect ? intersect->size() : 0;
	inner::axis_aligned_bounding_box aabb(get_local_scale(), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
	bool contact = aabb.intersect_sphere_cast(local_start, local_end, radius, intersect);
	if (contact && intersect)
		intersect->multiply_matrices(begin, get_transeform_matrix());

	return contact;
}

bool bounding_box::intersect_capsule_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
										  float radius, float length, const DirectX::XMFLOAT4& rotation,
										  intersect_data* intersect) const
{
	DirectX::XMFLOAT3 local_start, local_end;
	DirectX::XMStoreFloat3(&local_start, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&start), get_inverse_transform_matrix()));
	DirectX::XMStoreFloat3(&local_end,   DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&end),   get_inverse_transform_matrix()));

	DirectX::XMVECTOR WorldAxis = DirectX::XMVector3Normalize(DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XMLoadFloat4(&rotation)));
	DirectX::XMVECTOR LocalAxis = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(WorldAxis, get_inverse_transform_matrix()));

	size_t begin = intersect ? intersect->size() : 0;
	inner::axis_aligned_bounding_box aabb(get_local_scale(), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
	bool contact = aabb.intersect_capsule_cast_by_axis_spheres(local_start, local_end, radius, length, LocalAxis, intersect);
	if (contact && intersect)
		intersect->multiply_matrices(begin, get_transeform_matrix());
	return contact;
}

void bounding_box::include_corner(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max) const
{
	DirectX::XMFLOAT3 vertex[8];
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

bounding_box bounding_box::multiply_obb(const DirectX::XMMATRIX& matrix) const
{
	//	指定行列の空間へ変換
	DirectX::XMMATRIX LocalMatrix = DirectX::XMMatrixMultiply(get_transform_matrix(), DirectX::XMMatrixInverse(nullptr, matrix));

	DirectX::XMVECTOR Scale, Rotation, Position;
	DirectX::XMMatrixDecompose(&Scale, &Rotation, &Position, LocalMatrix);
	DirectX::XMFLOAT4 rotation;
	DirectX::XMStoreFloat4(&rotation, Rotation);
	DirectX::XMFLOAT3 position;
	DirectX::XMStoreFloat3(&position, Position);
	return bounding_box(get_local_scale(), rotation, position);
}

bounding_box bounding_box::multiply_obb(const DirectX::XMFLOAT4X4& matrix) const
{
	return multiply_obb(DirectX::XMLoadFloat4x4(&matrix));
}

void bounding_box::calculate_vertex(DirectX::XMFLOAT3* vertices, size_t vertex_count) const
{
	if (!vertices)
		return;

	DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&get_rotation()));
	RotationMatrix.r[0] = DirectX::XMVectorMultiply(RotationMatrix.r[0], DirectX::XMVectorSet(get_local_scale().x, get_local_scale().x, get_local_scale().x, 1.0f));
	RotationMatrix.r[1] = DirectX::XMVectorMultiply(RotationMatrix.r[1], DirectX::XMVectorSet(get_local_scale().y, get_local_scale().y, get_local_scale().y, 1.0f));
	RotationMatrix.r[2] = DirectX::XMVectorMultiply(RotationMatrix.r[2], DirectX::XMVectorSet(get_local_scale().z, get_local_scale().z, get_local_scale().z, 1.0f));

	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Vertices[8];
	DirectX::XMVECTOR PnX = DirectX::XMVectorSubtract(Position, RotationMatrix.r[0]);
	DirectX::XMVECTOR PpX = DirectX::XMVectorAdd(Position, RotationMatrix.r[0]);
	DirectX::XMVECTOR YnZ = DirectX::XMVectorSubtract(RotationMatrix.r[1], RotationMatrix.r[2]);
	DirectX::XMVECTOR YpZ = DirectX::XMVectorAdd(RotationMatrix.r[1], RotationMatrix.r[2]);
	Vertices[0]	= DirectX::XMVectorAdd(PnX, YnZ);
	Vertices[1]	= DirectX::XMVectorAdd(PpX, YnZ);
	Vertices[2] = DirectX::XMVectorAdd(PpX, YpZ);
	Vertices[3]	= DirectX::XMVectorAdd(PnX, YpZ);

	Vertices[4] = DirectX::XMVectorSubtract(PnX, YpZ);
	Vertices[5] = DirectX::XMVectorSubtract(PpX, YpZ);
	Vertices[6] = DirectX::XMVectorSubtract(PpX, YnZ);
	Vertices[7] = DirectX::XMVectorSubtract(PnX, YnZ);
	for (size_t index = 0; index < vertex_count; ++index)
	{
		DirectX::XMStoreFloat3(vertices + index, Vertices[index]);
	}
}
