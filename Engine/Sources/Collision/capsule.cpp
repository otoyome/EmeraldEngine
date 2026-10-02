#include	"collision/capsule.h"

#include	"collision/sphere.h"
#include	"collision/box.h"
#include	"collision/mesh.h"

using namespace collision;

float bounding_capsule::get_radius() const
{
	return get_local_scale().x;
}

void bounding_capsule::set_radius(float radius)
{
	set_local_scale({fabsf(radius), get_length(), 0.0f});
}

float bounding_capsule::get_length() const
{
	return get_local_scale().y;
}

void bounding_capsule::set_length(float length)
{
	set_local_scale({get_radius(), fabsf(length), 0.0f});
}

DirectX::XMVECTOR bounding_capsule::get_axis_vector() const
{
	return DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&get_rotation())).r[1];
}

DirectX::XMFLOAT3 bounding_capsule::get_axis_float3() const
{
	DirectX::XMFLOAT3 vector;
	DirectX::XMStoreFloat3(&vector, get_axis_vector());
	return vector;
}

bounding_capsule::bounding_capsule()
: bounding_object()
{
	//	線分の再計算
	on_change_parameter();
}

bounding_capsule::bounding_capsule(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& rotation, float radius, float length)
: bounding_object({radius, length, 0.0f}, rotation, position)
{
	//	線分の再計算
	on_change_parameter();
}

bounding_capsule::~bounding_capsule()
{
}

bool bounding_capsule::intersect_point(const DirectX::XMFLOAT3& point) const
{
	DirectX::XMFLOAT3 horizon;
	float delta;
	return line.calculate_distance(horizon, delta, point) - get_radius() <= 0.0f;
}

bool bounding_capsule::intersect_segment(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, intersect_data* intersect) const
{
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
	DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);
	DirectX::XMVECTOR Line0 = DirectX::XMLoadFloat3(&line[0]);
	DirectX::XMVECTOR Line1 = DirectX::XMLoadFloat3(&line[1]);
	const float radius = get_radius();

	//	軸上の最近点
	auto axis_point = [&](DirectX::XMVECTOR Point)
	{
		DirectX::XMFLOAT3 point, horizon;
		float delta;
		DirectX::XMStoreFloat3(&point, Point);
		line.calculate_distance(horizon, delta, point);
		return DirectX::XMLoadFloat3(&horizon);
	};

	//	始点が内側
	DirectX::XMVECTOR StartAxis = axis_point(Start);
	DirectX::XMVECTOR StartVector = DirectX::XMVectorSubtract(Start, StartAxis);
	float start_length = DirectX::XMVectorGetX(DirectX::XMVector3Length(StartVector));
	if (start_length <= radius)
	{
		if (intersect)
		{
			DirectX::XMVECTOR Normal = (start_length > 1e-6f) ? DirectX::XMVectorScale(StartVector, 1.0f / start_length)
															  : DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(Start, End));
			DirectX::XMFLOAT3 normal;
			DirectX::XMStoreFloat3(&normal, Normal);
			intersect->add(start, normal, 0.0f);
		}
		return true;
	}

	//	両端の球と円柱で最も近い交点
	DirectX::XMVECTOR HitPosition;
	float hit_distance = 0.0f;
	float near_distance = FLT_MAX;
	DirectX::XMVECTOR NearPosition = DirectX::XMVectorZero();
	if (intersect_ray_vs_sphere(Start, End, Line0, radius, HitPosition, hit_distance) && hit_distance < near_distance)
	{
		near_distance = hit_distance;
		NearPosition = HitPosition;
	}
	if (intersect_ray_vs_sphere(Start, End, Line1, radius, HitPosition, hit_distance) && hit_distance < near_distance)
	{
		near_distance = hit_distance;
		NearPosition = HitPosition;
	}
	if (intersect_ray_vs_cylinder(Start, End, Line0, Line1, radius, HitPosition, hit_distance) && hit_distance < near_distance)
	{
		near_distance = hit_distance;
		NearPosition = HitPosition;
	}
	if (near_distance == FLT_MAX)
		return false;

	if (intersect)
	{
		DirectX::XMVECTOR Normal = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(NearPosition, axis_point(NearPosition)));
		DirectX::XMFLOAT3 position, normal;
		DirectX::XMStoreFloat3(&position, NearPosition);
		DirectX::XMStoreFloat3(&normal, Normal);
		intersect->add(position, normal, near_distance);
	}
	return true;
}

bool bounding_capsule::intersect_sphere(const DirectX::XMFLOAT3& point, float radius, intersect_data* intersect) const
{
	DirectX::XMFLOAT3 horizon;
	float delta;
	float length = line.calculate_distance(horizon, delta, point) - get_radius() - radius;

	if( intersect && length <= 0.0f )
	{
		//	軸上の最近点から自分を押し出す向き
		DirectX::XMVECTOR Point = DirectX::XMLoadFloat3(&point);
		DirectX::XMVECTOR Horizon = DirectX::XMLoadFloat3(&horizon);
		DirectX::XMVECTOR Vector = DirectX::XMVectorSubtract(Horizon, Point);
		DirectX::XMVECTOR Normal = (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Vector)) > 1e-12f)
			? DirectX::XMVector3Normalize(Vector)
			: DirectX::XMVector3Normalize(DirectX::XMVector3Orthogonal(get_axis_vector()));

		DirectX::XMFLOAT3 insert_point, normal;
		DirectX::XMStoreFloat3(&normal, Normal);
		DirectX::XMStoreFloat3(&insert_point, DirectX::XMVectorSubtract(Horizon, DirectX::XMVectorScale(Normal, get_radius())));
		intersect->add(insert_point, normal, -length);
	}

	return length <= 0.0f;
}

bool bounding_capsule::intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect) const
{
	return	intersect_sphere(sphere.get_position(), sphere.get_radius(), intersect);
}

bool bounding_capsule::intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect) const
{
	//	先に球同士で判定
	bounding_sphere self_bs = bounding_sphere(get_position(), get_radius() + get_length() * 0.5f);
	bounding_sphere capsule_bs = bounding_sphere(capsule.get_position(), capsule.get_radius() + capsule.get_length() * 0.5f);
	if (!self_bs.intersect_sphere(capsule_bs))
		return false;

	//	カプセル同士の当たり判定
	DirectX::XMFLOAT3 horizon1, horizon2;
	float delta1, delta2;
	float length = line.calculate_distance(horizon1, delta1, horizon2, delta2, capsule.line) - (get_radius() + capsule.get_radius());

	if (intersect && length <= 0.0f)
	{
		//	相手から自分へ向かう向き
		DirectX::XMVECTOR Horizon1 = DirectX::XMLoadFloat3(&horizon1);
		DirectX::XMVECTOR Horizon2 = DirectX::XMLoadFloat3(&horizon2);
		DirectX::XMVECTOR Vector = DirectX::XMVectorSubtract(Horizon1, Horizon2);
		DirectX::XMVECTOR Normal;
		if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Vector)) > 1e-12f)
		{
			Normal = DirectX::XMVector3Normalize(Vector);
		}
		else
		{
			//	軸が交差している場合は両軸に垂直
			Normal = DirectX::XMVector3Cross(get_axis_vector(), capsule.get_axis_vector());
			if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Normal)) <= 1e-12f)
				Normal = DirectX::XMVector3Orthogonal(get_axis_vector());
			Normal = DirectX::XMVector3Normalize(Normal);
		}

		DirectX::XMFLOAT3 insert_point, normal;
		DirectX::XMStoreFloat3(&normal, Normal);
		DirectX::XMStoreFloat3(&insert_point, DirectX::XMVectorSubtract(Horizon1, DirectX::XMVectorScale(Normal, get_radius())));
		intersect->add(insert_point, normal, -length);
	}

	return length <= 0.0f;
}

bool bounding_capsule::intersect_box(const bounding_box& box, intersect_data* intersect) const
{
	return delegate_flipped(intersect, [&]() { return box.intersect_capsule(*this, intersect); });
}

bool bounding_capsule::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	return delegate_flipped(intersect, [&]() { return mesh.intersect_capsule(*this, intersect); });
}

bool bounding_capsule::intersect_sphere_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float radius, intersect_data* intersect) const
{
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
	DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);
	DirectX::XMVECTOR Direction = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(End, Start));

	DirectX::XMVECTOR SphereCenter0 = DirectX::XMLoadFloat3(&line[0]);
	DirectX::XMVECTOR SphereCenter1 = DirectX::XMLoadFloat3(&line[1]);
	const float combined_radius = get_radius() + radius;

	//	軸上の最近点
	auto axis_point = [&](DirectX::XMVECTOR Point)
	{
		DirectX::XMFLOAT3 point, horizon;
		float delta;
		DirectX::XMStoreFloat3(&point, Point);
		line.calculate_distance(horizon, delta, point);
		return DirectX::XMLoadFloat3(&horizon);
	};

	//	開始時の重なり
	{
		DirectX::XMVECTOR AxisPoint = axis_point(Start);
		DirectX::XMVECTOR ToStart = DirectX::XMVectorSubtract(Start, AxisPoint);
		float start_distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(ToStart));
		if (start_distance < combined_radius)
		{
			DirectX::XMVECTOR Normal = (start_distance > 1e-6f) ? DirectX::XMVectorScale(ToStart, 1.0f / start_distance) : DirectX::XMVectorNegate(Direction);
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(Direction, Normal)) >= 0.0f)
				return false;
			add_cast_hit(intersect, Start, DirectX::XMVectorAdd(AxisPoint, DirectX::XMVectorScale(Normal, get_radius())), Normal, 0.0f);
			return true;
		}
	}

	DirectX::XMVECTOR NearCenter = DirectX::XMVectorZero(), HitPosition;
	float near_distance = FLT_MAX, hit_distance;
	if (intersect_ray_vs_sphere(Start, End, SphereCenter0, combined_radius, HitPosition, hit_distance) && hit_distance > 0.0f && hit_distance < near_distance)
	{
		near_distance = hit_distance;
		NearCenter = HitPosition;
	}
	if (intersect_ray_vs_sphere(Start, End, SphereCenter1, combined_radius, HitPosition, hit_distance) && hit_distance > 0.0f && hit_distance < near_distance)
	{
		near_distance = hit_distance;
		NearCenter = HitPosition;
	}
	if (intersect_ray_vs_cylinder(Start, End, SphereCenter0, SphereCenter1, combined_radius, HitPosition, hit_distance) && hit_distance > 0.0f && hit_distance < near_distance)
	{
		near_distance = hit_distance;
		NearCenter = HitPosition;
	}
	if (near_distance == FLT_MAX)
		return false;

	if (intersect)
	{
		//	軸上の最近点から接触時の中心へ向かう外向き法線
		DirectX::XMVECTOR AxisPoint = axis_point(NearCenter);
		DirectX::XMVECTOR Normal = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(NearCenter, AxisPoint));
		DirectX::XMVECTOR Contact = DirectX::XMVectorAdd(AxisPoint, DirectX::XMVectorScale(Normal, get_radius()));
		add_cast_hit(intersect, NearCenter, Contact, Normal, near_distance);
	}
	return true;
}

bool bounding_capsule::intersect_capsule_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
											  float radius, float length, const DirectX::XMFLOAT4& rotation,
											  intersect_data* intersect) const
{
	DirectX::XMVECTOR axis = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XMLoadFloat4(&rotation));
	return intersect_capsule_cast_by_axis_spheres(start, end, radius, length, axis, intersect);
}

void bounding_capsule::include_corner(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max) const
{
	DirectX::XMVECTOR LinePositionA = DirectX::XMLoadFloat3(&line[0]);
	DirectX::XMVECTOR LinePositionB = DirectX::XMLoadFloat3(&line[1]);
	DirectX::XMVECTOR Radius = DirectX::XMVectorSet(get_radius(), get_radius(), get_radius(), get_radius());
	DirectX::XMVECTOR Vertex[4] =
	{
		DirectX::XMVectorSubtract(LinePositionA, Radius),
		DirectX::XMVectorSubtract(LinePositionB, Radius),
		DirectX::XMVectorAdd(LinePositionA, Radius),
		DirectX::XMVectorAdd(LinePositionB, Radius),
	};
	DirectX::XMVECTOR Min = Vertex[0];
	DirectX::XMVECTOR Max = Vertex[0];
	for (size_t index = 1; index < 4; ++index)
	{
		Min = DirectX::XMVectorMin(Min, Vertex[index]);
		Max = DirectX::XMVectorMax(Max, Vertex[index]);
	}
	DirectX::XMStoreFloat3(&min, Min);
	DirectX::XMStoreFloat3(&max, Max);
}

void bounding_capsule::on_change_parameter()
{
	bounding_object::on_change_parameter();

	DirectX::XMVECTOR Rotation = DirectX::XMLoadFloat4(&get_rotation());
	DirectX::XMMATRIX Matrix = DirectX::XMMatrixRotationQuaternion(Rotation);

	float length = get_length() * 0.5f;
	DirectX::XMVECTOR Line = DirectX::XMVectorMultiply(Matrix.r[1], DirectX::XMVectorSet(length, length, length, length));
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Start = DirectX::XMVectorSubtract(Position, Line);
	DirectX::XMVECTOR End = DirectX::XMVectorAdd(Position, Line);
	DirectX::XMFLOAT3 start, end;
	DirectX::XMStoreFloat3(&start, Start);
	DirectX::XMStoreFloat3(&end, End);
	line = decltype(line)(line::types::segment, start, end);

}
