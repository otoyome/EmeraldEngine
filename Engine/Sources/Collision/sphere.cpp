#include	"collision/sphere.h"

#include	"collision/capsule.h"
#include	"collision/box.h"
#include	"collision/mesh.h"

#include	<DirectXCollision.h>

using namespace collision;


float bounding_sphere::get_radius() const
{
	return get_local_scale().x;
}

void bounding_sphere::set_radius(float radius)
{
	set_local_scale({radius, 0.0f, 0.0f});
}

bounding_sphere::bounding_sphere()
: bounding_object()
{
}

bounding_sphere::bounding_sphere(const DirectX::XMFLOAT3& position, float radius)
: bounding_object({radius, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, position)
{
}

bounding_sphere::~bounding_sphere()
{
}

bool bounding_sphere::intersect_point(const DirectX::XMFLOAT3& point) const
{
	DirectX::XMVECTOR Point = DirectX::XMLoadFloat3(&point);
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Vector = DirectX::XMVectorSubtract(Position, Point);
	DirectX::XMVECTOR Distance = DirectX::XMVector3Length(Vector);
	return	DirectX::XMVectorGetX(Distance) - get_radius() <= 0.0f;
}

bool bounding_sphere::intersect_segment(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, intersect_data* intersect) const
{
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
	DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);

	return intersect_segment(Start, End, intersect);
}

bool bounding_sphere::intersect_segment(DirectX::XMVECTOR Start, DirectX::XMVECTOR End, intersect_data* intersect) const
{
	DirectX::XMVECTOR Direction = DirectX::XMVectorSubtract(End, Start);
	float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(Direction));
	if (distance <= 1e-6f)
		return false;
	Direction = DirectX::XMVectorScale(Direction, 1.0f / distance);
	const float segment_length = distance;

	DirectX::XMVECTOR Center = DirectX::XMLoadFloat3(&get_position());
	const float radius = get_radius();

	//	始点が内側
	DirectX::XMVECTOR StartToCenter = DirectX::XMVectorSubtract(Start, Center);
	float start_length_sq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(StartToCenter));
	if (start_length_sq <= radius * radius)
	{
		if (intersect)
		{
			DirectX::XMVECTOR Normal = (start_length_sq > 1e-12f) ? DirectX::XMVector3Normalize(StartToCenter) : DirectX::XMVectorNegate(Direction);
			DirectX::XMFLOAT3 position, normal;
			DirectX::XMStoreFloat3(&position, Start);
			DirectX::XMStoreFloat3(&normal, Normal);
			intersect->add(position, normal, 0.0f);
		}
		return true;
	}

	DirectX::BoundingSphere sphere = DirectX::BoundingSphere(get_position(), radius);
	if (sphere.Intersects(Start, Direction, distance) && distance <= segment_length)
	{
		if (intersect)
		{
			DirectX::XMVECTOR Position = DirectX::XMVectorMultiplyAdd(Direction, DirectX::XMVectorReplicate(distance), Start);
			DirectX::XMFLOAT3 position, normal;
			DirectX::XMStoreFloat3(&position, Position);
			DirectX::XMStoreFloat3(&normal, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(Position, Center)));
			intersect->add(position, normal, distance);
		}
		return true;
	}
	return false;
}

bool bounding_sphere::intersect_sphere(const DirectX::XMFLOAT3& point, float radius, intersect_data* intersect) const
{
	DirectX::XMVECTOR Point = DirectX::XMLoadFloat3(&point);
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Vector = DirectX::XMVectorSubtract(Position, Point);
	DirectX::XMVECTOR Distance = DirectX::XMVector3Length(Vector);
	float distance = DirectX::XMVectorGetX(Distance) - get_radius() - radius;
	bool insert = distance <= 0.0f;
	if (intersect && insert)
	{
		//	中心一致時は上方向
		if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Vector)) <= 1e-12f)
			Vector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		Vector = DirectX::XMVector3Normalize(Vector);
		DirectX::XMFLOAT3 point, normal;
		DirectX::XMStoreFloat3(&normal, Vector);
		radius += distance;
		Distance = DirectX::XMVectorSet(radius, radius, radius, radius);
		DirectX::XMStoreFloat3(&point, DirectX::XMVectorMultiplyAdd(Vector, Distance, Point));
		intersect->add(point, normal, -distance);
	}
	return insert;
}

bool bounding_sphere::intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect) const
{
	return intersect_sphere(sphere.get_position(), sphere.get_radius(), intersect);
}

bool bounding_sphere::intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect) const
{
	return delegate_flipped(intersect, [&]() { return capsule.intersect_sphere(*this, intersect); });
}

bool bounding_sphere::intersect_box(const bounding_box& box, intersect_data* intersect) const
{
	return delegate_flipped(intersect, [&]() { return box.intersect_sphere(*this, intersect); });
}

bool bounding_sphere::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	return delegate_flipped(intersect, [&]() { return mesh.intersect_sphere(*this, intersect); });
}


bool bounding_sphere::intersect_sphere_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float radius, intersect_data* intersect) const
{
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
	DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);
	DirectX::XMVECTOR SphereCenter = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Direction = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(End, Start));

	//	開始時の重なり
	{
		DirectX::XMVECTOR ToStart = DirectX::XMVectorSubtract(Start, SphereCenter);
		float start_distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(ToStart));
		if (start_distance < get_radius() + radius)
		{
			DirectX::XMVECTOR Normal = (start_distance > 1e-6f) ? DirectX::XMVectorScale(ToStart, 1.0f / start_distance) : DirectX::XMVectorNegate(Direction);
			if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(Direction, Normal)) >= 0.0f)
				return false;
			add_cast_hit(intersect, Start, DirectX::XMVectorAdd(SphereCenter, DirectX::XMVectorScale(Normal, get_radius())), Normal, 0.0f);
			return true;
		}
	}

	DirectX::XMVECTOR HitPosition;
	float hit_distance;
	if (intersect_ray_vs_sphere(Start, End, SphereCenter, get_radius() + radius, HitPosition, hit_distance) && hit_distance > 0.0f)
	{
		DirectX::XMVECTOR Normal = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(HitPosition, SphereCenter));
		DirectX::XMVECTOR Contact = DirectX::XMVectorSubtract(HitPosition, DirectX::XMVectorScale(Normal, radius));
		add_cast_hit(intersect, HitPosition, Contact, Normal, hit_distance);
		return true;
	}

	return false;
}

bool bounding_sphere::intersect_capsule_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
											 float radius, float length, const DirectX::XMFLOAT4& rotation,
											 intersect_data* intersect) const
{
	using namespace DirectX;

	XMVECTOR SphereCenter = XMLoadFloat3(&get_position());
	XMVECTOR Start = XMLoadFloat3(&start);
	XMVECTOR End = XMLoadFloat3(&end);

	XMVECTOR Velocity = XMVectorSubtract(End, Start);
	float velocity_length = XMVectorGetX(XMVector3Length(Velocity));
	if (velocity_length < FLT_EPSILON)
		return false;
	XMVECTOR VelocityNormal = XMVectorScale(Velocity, 1.0f / velocity_length);

	//	カプセルの軸端点
	XMVECTOR Axis = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMLoadFloat4(&rotation)));
	float half = length * 0.5f;
	XMVECTOR Cap0 = XMVectorScale(Axis, -half);
	XMVECTOR Cap1 = XMVectorScale(Axis, +half);

	float combined_radius = get_radius() + radius;

	//	軸上の最近点
	auto axis_point = [&](XMVECTOR Center)
	{
		float proj = XMVectorGetX(XMVector3Dot(XMVectorSubtract(SphereCenter, Center), Axis));
		proj = math::clamp(proj, -half, +half);
		return XMVectorAdd(Center, XMVectorScale(Axis, proj));
	};

	//	開始時の重なり
	{
		XMVECTOR AxisPoint = axis_point(Start);
		XMVECTOR ToCaster = XMVectorSubtract(AxisPoint, SphereCenter);
		float start_distance = XMVectorGetX(XMVector3Length(ToCaster));
		if (start_distance < combined_radius)
		{
			XMVECTOR Normal = (start_distance > 1e-6f) ? XMVectorScale(ToCaster, 1.0f / start_distance) : XMVectorNegate(VelocityNormal);
			if (XMVectorGetX(XMVector3Dot(VelocityNormal, Normal)) >= 0.0f)
				return false;
			add_cast_hit(intersect, Start, XMVectorAdd(SphereCenter, XMVectorScale(Normal, get_radius())), Normal, 0.0f);
			return true;
		}
	}

	//	移動カプセル基準フレームでの対象球の光線
	XMVECTOR RayStart = XMVectorSubtract(SphereCenter, Start);
	XMVECTOR RayEnd = XMVectorSubtract(SphereCenter, End);

	XMVECTOR HitPosition;
	float hit_distance;
	float nearest = FLT_MAX;

	if (intersect_ray_vs_sphere(RayStart, RayEnd, Cap0, combined_radius, HitPosition, hit_distance) && hit_distance > 0.0f)
		if (hit_distance < nearest) nearest = hit_distance;
	if (intersect_ray_vs_sphere(RayStart, RayEnd, Cap1, combined_radius, HitPosition, hit_distance) && hit_distance > 0.0f)
		if (hit_distance < nearest) nearest = hit_distance;
	if (intersect_ray_vs_cylinder(RayStart, RayEnd, Cap0, Cap1, combined_radius, HitPosition, hit_distance) && hit_distance > 0.0f)
		if (hit_distance < nearest) nearest = hit_distance;

	if (nearest == FLT_MAX)
		return false;

	if (intersect)
	{
		XMVECTOR CenterWorld = XMVectorAdd(Start, XMVectorScale(VelocityNormal, nearest));
		XMVECTOR AxisPointWorld = axis_point(CenterWorld);

		//	対象球の外向き法線
		XMVECTOR Direction = XMVectorSubtract(AxisPointWorld, SphereCenter);
		if (XMVectorGetX(XMVector3LengthSq(Direction)) < FLT_EPSILON)
			Direction = XMVectorNegate(VelocityNormal);
		XMVECTOR Normal = XMVector3Normalize(Direction);
		XMVECTOR ContactPoint = XMVectorAdd(SphereCenter, XMVectorScale(Normal, get_radius()));
		add_cast_hit(intersect, CenterWorld, ContactPoint, Normal, nearest);
	}
	return true;
}

void bounding_sphere::include_corner(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max) const
{
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&get_position());
	DirectX::XMVECTOR Radius = DirectX::XMVectorSet(get_radius(), get_radius(), get_radius(), 0);
	DirectX::XMStoreFloat3(&min, DirectX::XMVectorSubtract(Position, Radius));
	DirectX::XMStoreFloat3(&max, DirectX::XMVectorAdd(Position, Radius));
}
