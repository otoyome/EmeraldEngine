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
	Direction = DirectX::XMVector3Normalize(Direction);
	const float segment_length = distance;

	DirectX::BoundingSphere sphere = DirectX::BoundingSphere(get_position(), get_radius());
	if (sphere.Intersects(Start, Direction, distance) && distance <= segment_length)
	{
		if (intersect)
		{
			DirectX::XMVECTOR Distance = DirectX::XMVectorSet(distance, distance, distance, distance);
			DirectX::XMFLOAT3 position, direction;
			DirectX::XMStoreFloat3(&position, DirectX::XMVectorMultiplyAdd(Direction, Distance, Start));
			DirectX::XMStoreFloat3(&direction, DirectX::XMVectorNegate(Direction));
			intersect->add(position, direction, distance);
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
	return	capsule.intersect_sphere(*this, intersect);
}

bool bounding_sphere::intersect_box(const bounding_box& box, intersect_data* intersect) const
{
	return	box.intersect_sphere(*this, intersect);
}

bool bounding_sphere::intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect) const
{
	return	mesh.intersect_sphere(*this, intersect);
}


bool bounding_sphere::intersect_sphere_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float radius, intersect_data* intersect) const
{
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
	DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);
	DirectX::XMVECTOR SphereCenter = DirectX::XMLoadFloat3(&get_position());
 
	DirectX::XMVECTOR HitPosition;
	float hit_distance;
	if (intersect_ray_vs_sphere(Start, End, SphereCenter, get_radius() + radius, HitPosition, hit_distance) && hit_distance > 0.0f)
	{
		DirectX::XMVECTOR Vec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(SphereCenter, HitPosition));
		HitPosition = DirectX::XMVectorAdd(HitPosition, DirectX::XMVectorScale(Vec, radius));
		DirectX::XMFLOAT3 point, normal;
		DirectX::XMStoreFloat3(&point, HitPosition);
		DirectX::XMStoreFloat3(&normal, Vec);
		intersect->add(point, normal, hit_distance);
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
		XMVECTOR ToSphere = XMVectorSubtract(SphereCenter, CenterWorld);
		float proj = XMVectorGetX(XMVector3Dot(ToSphere, Axis));
		if (proj < -half) proj = -half;
		if (proj > +half) proj = +half;
		XMVECTOR AxisPointWorld = XMVectorAdd(CenterWorld, XMVectorScale(Axis, proj));

		XMVECTOR Direction = XMVectorSubtract(SphereCenter, AxisPointWorld);
		if (XMVectorGetX(XMVector3LengthSq(Direction)) < FLT_EPSILON)
			Direction = VelocityNormal;
		XMVECTOR Normal = XMVector3Normalize(Direction);
		XMVECTOR ContactPoint = XMVectorAdd(AxisPointWorld, XMVectorScale(Normal, radius));

		DirectX::XMFLOAT3 point, normal;
		XMStoreFloat3(&point, ContactPoint);
		XMStoreFloat3(&normal, Normal);
		intersect->add(point, normal, nearest);
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
