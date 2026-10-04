#include	"collision/sphere.h"

#include	"collision/capsule.h"
#include	"collision/box.h"
#include	"collision/mesh.h"


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

bounding_sphere::bounding_sphere(const math::float3& position, float radius)
: bounding_object({radius, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, position)
{
}

bounding_sphere::~bounding_sphere()
{
}

bool bounding_sphere::intersect_point(const math::float3& point) const
{
	math::vector3f Point(point);
	math::vector3f Position(get_position());
	math::vector3f Vector = Position - Point;
	float Distance = Vector.length();
	return	Distance - get_radius() <= 0.0f;
}

bool bounding_sphere::intersect_segment(const math::float3& start, const math::float3& end, intersect_data* intersect) const
{
	math::vector3f Start(start);
	math::vector3f End(end);

	return intersect_segment(Start, End, intersect);
}

bool bounding_sphere::intersect_segment(math::vector3f Start, math::vector3f End, intersect_data* intersect) const
{
	math::vector3f Direction = End - Start;
	float distance = Direction.length();
	if (distance <= 1e-6f)
		return false;
	Direction = Direction * (1.0f / distance);
	const float segment_length = distance;

	math::vector3f Center(get_position());
	const float radius = get_radius();

	//	始点が内側
	math::vector3f StartToCenter = Start - Center;
	float start_length_sq = StartToCenter.square_length();
	if (start_length_sq <= radius * radius)
	{
		if (intersect)
		{
			math::vector3f Normal = (start_length_sq > 1e-12f) ? StartToCenter.normalized() : -Direction;
			math::float3 position, normal;
			position = Start.to_float3();
			normal = Normal.to_float3();
			intersect->add(position, normal, 0.0f);
		}
		return true;
	}

	//	始点からのレイと球の交差
	const math::vector3f to_center = Center - Start;
	const float projection = math::vector3f::dot(to_center, Direction);
	const float to_center_length_sq = to_center.square_length();
	const float radius_sq = radius * radius;
	const float distance_sq = to_center_length_sq - projection * projection;
	bool ray_hit = !(projection < 0.0f && to_center_length_sq > radius_sq) && distance_sq <= radius_sq;
	if (ray_hit)
	{
		const float q = std::sqrt(radius_sq - distance_sq);
		distance = (to_center_length_sq > radius_sq) ? projection - q : projection + q;
	}
	if (ray_hit && distance <= segment_length)
	{
		if (intersect)
		{
			math::vector3f Position = Direction * distance + Start;
			math::float3 position, normal;
			position = Position.to_float3();
			normal = ((Position - Center).normalized()).to_float3();
			intersect->add(position, normal, distance);
		}
		return true;
	}
	return false;
}

bool bounding_sphere::intersect_sphere(const math::float3& point, float radius, intersect_data* intersect) const
{
	math::vector3f Point(point);
	math::vector3f Position(get_position());
	math::vector3f Vector = Position - Point;
	float Distance = Vector.length();
	float distance = Distance - get_radius() - radius;
	bool insert = distance <= 0.0f;
	if (intersect && insert)
	{
		//	中心一致時は上方向
		if (Vector.square_length() <= 1e-12f)
			Vector = math::vector3f(0.0f, 1.0f, 0.0f);
		Vector = Vector.normalized();
		math::float3 point, normal;
		normal = Vector.to_float3();
		radius += distance;
		Distance = radius;
		point = (Vector * Distance + Point).to_float3();
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


bool bounding_sphere::intersect_sphere_cast(const math::float3& start, const math::float3& end, float radius, intersect_data* intersect) const
{
	math::vector3f Start(start);
	math::vector3f End(end);
	math::vector3f SphereCenter(get_position());
	math::vector3f Direction = (End - Start).normalized();

	//	開始時の重なり
	{
		math::vector3f ToStart = Start - SphereCenter;
		float start_distance = ToStart.length();
		if (start_distance < get_radius() + radius)
		{
			math::vector3f Normal = (start_distance > 1e-6f) ? ToStart * (1.0f / start_distance) : -Direction;
			if (math::vector3f::dot(Direction, Normal) >= 0.0f)
				return false;
			add_cast_hit(intersect, Start, SphereCenter + (Normal * get_radius()), Normal, 0.0f);
			return true;
		}
	}

	math::vector3f HitPosition;
	float hit_distance;
	if (intersect_ray_vs_sphere(Start, End, SphereCenter, get_radius() + radius, HitPosition, hit_distance) && hit_distance > 0.0f)
	{
		math::vector3f Normal = (HitPosition - SphereCenter).normalized();
		math::vector3f Contact = HitPosition - (Normal * radius);
		add_cast_hit(intersect, HitPosition, Contact, Normal, hit_distance);
		return true;
	}

	return false;
}

bool bounding_sphere::intersect_capsule_cast(const math::float3& start, const math::float3& end,
											 float radius, float length, const math::float4& rotation,
											 intersect_data* intersect) const
{

	math::vector3f SphereCenter(get_position());
	math::vector3f Start(start);
	math::vector3f End(end);

	math::vector3f Velocity = End - Start;
	float velocity_length = Velocity.length();
	if (velocity_length < FLT_EPSILON)
		return false;
	math::vector3f VelocityNormal = Velocity * (1.0f / velocity_length);

	//	カプセルの軸端点
	math::vector3f Axis = (math::quaternionf(rotation).rotate(math::vector3f(0.0f, 1.0f, 0.0f))).normalized();
	float half = length * 0.5f;
	math::vector3f Cap0 = Axis * -half;
	math::vector3f Cap1 = Axis * (+half);

	float combined_radius = get_radius() + radius;

	//	軸上の最近点
	auto axis_point = [&](math::vector3f Center)
	{
		float proj = math::vector3f::dot(SphereCenter - Center, Axis);
		proj = math::clamp(proj, -half, +half);
		return Center + (Axis * proj);
	};

	//	開始時の重なり
	{
		math::vector3f AxisPoint = axis_point(Start);
		math::vector3f ToCaster = AxisPoint - SphereCenter;
		float start_distance = ToCaster.length();
		if (start_distance < combined_radius)
		{
			math::vector3f Normal = (start_distance > 1e-6f) ? ToCaster * (1.0f / start_distance) : -VelocityNormal;
			if (math::vector3f::dot(VelocityNormal, Normal) >= 0.0f)
				return false;
			add_cast_hit(intersect, Start, SphereCenter + (Normal * get_radius()), Normal, 0.0f);
			return true;
		}
	}

	//	移動カプセル基準フレームでの対象球の光線
	math::vector3f RayStart = SphereCenter - Start;
	math::vector3f RayEnd = SphereCenter - End;

	math::vector3f HitPosition;
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
		math::vector3f CenterWorld = Start + (VelocityNormal * nearest);
		math::vector3f AxisPointWorld = axis_point(CenterWorld);

		//	対象球の外向き法線
		math::vector3f Direction = AxisPointWorld - SphereCenter;
		if (Direction.square_length() < FLT_EPSILON)
			Direction = -VelocityNormal;
		math::vector3f Normal = Direction.normalized();
		math::vector3f ContactPoint = SphereCenter + (Normal * get_radius());
		add_cast_hit(intersect, CenterWorld, ContactPoint, Normal, nearest);
	}
	return true;
}

void bounding_sphere::include_corner(math::float3& min, math::float3& max) const
{
	math::vector3f Position(get_position());
	math::vector3f Radius = math::vector3f(get_radius(), get_radius(), get_radius());
	min = (Position - Radius).to_float3();
	max = (Position + Radius).to_float3();
}
