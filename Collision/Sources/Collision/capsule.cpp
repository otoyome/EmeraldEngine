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

math::vector3f bounding_capsule::get_axis_vector() const
{
	return math::matrixf::rotation_quaternion(math::quaternionf(get_rotation())).to_vector3(1);
}

math::float3 bounding_capsule::get_axis_float3() const
{
	math::float3 vector;
	vector = math::vector3f(get_axis_vector()).to_float3();
	return vector;
}

bounding_capsule::bounding_capsule()
: bounding_object()
{
	//	線分の再計算
	on_change_parameter();
}

bounding_capsule::bounding_capsule(const math::float3& position, const math::float4& rotation, float radius, float length)
: bounding_object({radius, length, 0.0f}, rotation, position)
{
	//	線分の再計算
	on_change_parameter();
}

bounding_capsule::~bounding_capsule()
{
}

bool bounding_capsule::intersect_point(const math::float3& point) const
{
	math::float3 horizon;
	float delta;
	return line.calculate_distance(horizon, delta, point) - get_radius() <= 0.0f;
}

bool bounding_capsule::intersect_segment(const math::float3& start, const math::float3& end, intersect_data* intersect) const
{
	math::vector3f Start(start);
	math::vector3f End(end);
	math::vector3f Line0(line[0]);
	math::vector3f Line1(line[1]);
	const float radius = get_radius();

	//	軸上の最近点
	auto axis_point = [&](math::vector3f Point)
	{
		math::float3 point, horizon;
		float delta;
		point = Point.to_float3();
		line.calculate_distance(horizon, delta, point);
		return math::vector3f(horizon);
	};

	//	始点が内側
	math::vector3f StartAxis = axis_point(Start);
	math::vector3f StartVector = Start - StartAxis;
	float start_length = StartVector.length();
	if (start_length <= radius)
	{
		if (intersect)
		{
			math::vector3f Normal = (start_length > 1e-6f) ? StartVector * (1.0f / start_length)
															  : (Start - End).normalized();
			math::float3 normal;
			normal = Normal.to_float3();
			intersect->add(start, normal, 0.0f);
		}
		return true;
	}

	//	両端の球と円柱で最も近い交点
	math::vector3f HitPosition;
	float hit_distance = 0.0f;
	float near_distance = FLT_MAX;
	math::vector3f NearPosition = math::vector3f::zero();
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
		math::vector3f Normal = (NearPosition - axis_point(NearPosition)).normalized();
		math::float3 position, normal;
		position = NearPosition.to_float3();
		normal = Normal.to_float3();
		intersect->add(position, normal, near_distance);
	}
	return true;
}

bool bounding_capsule::intersect_sphere(const math::float3& point, float radius, intersect_data* intersect) const
{
	math::float3 horizon;
	float delta;
	float length = line.calculate_distance(horizon, delta, point) - get_radius() - radius;

	if( intersect && length <= 0.0f )
	{
		//	軸上の最近点から自分を押し出す向き
		math::vector3f Point(point);
		math::vector3f Horizon(horizon);
		math::vector3f Vector = Horizon - Point;
		math::vector3f Normal = (Vector.square_length() > 1e-12f)
			? Vector.normalized()
			: math::vector3f::orthogonal(get_axis_vector()).normalized();

		math::float3 insert_point, normal;
		normal = Normal.to_float3();
		insert_point = (Horizon - (Normal * get_radius())).to_float3();
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
	math::float3 horizon1, horizon2;
	float delta1, delta2;
	float length = line.calculate_distance(horizon1, delta1, horizon2, delta2, capsule.line) - (get_radius() + capsule.get_radius());

	if (intersect && length <= 0.0f)
	{
		//	相手から自分へ向かう向き
		math::vector3f Horizon1(horizon1);
		math::vector3f Horizon2(horizon2);
		math::vector3f Vector = Horizon1 - Horizon2;
		math::vector3f Normal;
		if (Vector.square_length() > 1e-12f)
		{
			Normal = Vector.normalized();
		}
		else
		{
			//	軸が交差している場合は両軸に垂直
			Normal = math::vector3f::cross(get_axis_vector(), capsule.get_axis_vector());
			if (Normal.square_length() <= 1e-12f)
				Normal = math::vector3f::orthogonal(get_axis_vector());
			Normal = Normal.normalized();
		}

		math::float3 insert_point, normal;
		normal = Normal.to_float3();
		insert_point = (Horizon1 - (Normal * get_radius())).to_float3();
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

bool bounding_capsule::intersect_sphere_cast(const math::float3& start, const math::float3& end, float radius, intersect_data* intersect) const
{
	math::vector3f Start(start);
	math::vector3f End(end);
	math::vector3f Direction = (End - Start).normalized();

	math::vector3f SphereCenter0(line[0]);
	math::vector3f SphereCenter1(line[1]);
	const float combined_radius = get_radius() + radius;

	//	軸上の最近点
	auto axis_point = [&](math::vector3f Point)
	{
		math::float3 point, horizon;
		float delta;
		point = Point.to_float3();
		line.calculate_distance(horizon, delta, point);
		return math::vector3f(horizon);
	};

	//	開始時の重なり
	{
		math::vector3f AxisPoint = axis_point(Start);
		math::vector3f ToStart = Start - AxisPoint;
		float start_distance = ToStart.length();
		if (start_distance < combined_radius)
		{
			math::vector3f Normal = (start_distance > 1e-6f) ? ToStart * (1.0f / start_distance) : -Direction;
			if (math::vector3f::dot(Direction, Normal) >= 0.0f)
				return false;
			add_cast_hit(intersect, Start, AxisPoint + (Normal * get_radius()), Normal, 0.0f);
			return true;
		}
	}

	math::vector3f NearCenter = math::vector3f::zero(), HitPosition;
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
		math::vector3f AxisPoint = axis_point(NearCenter);
		math::vector3f Normal = (NearCenter - AxisPoint).normalized();
		math::vector3f Contact = AxisPoint + (Normal * get_radius());
		add_cast_hit(intersect, NearCenter, Contact, Normal, near_distance);
	}
	return true;
}

bool bounding_capsule::intersect_capsule_cast(const math::float3& start, const math::float3& end,
											  float radius, float length, const math::float4& rotation,
											  intersect_data* intersect) const
{
	math::vector3f axis = math::quaternionf(rotation).rotate(math::vector3f(0.0f, 1.0f, 0.0f));
	return intersect_capsule_cast_by_axis_spheres(start, end, radius, length, axis, intersect);
}

void bounding_capsule::include_corner(math::float3& min, math::float3& max) const
{
	math::vector3f LinePositionA(line[0]);
	math::vector3f LinePositionB(line[1]);
	const math::vector3f Radius(get_radius(), get_radius(), get_radius());
	math::vector3f Vertex[4] =
	{
		LinePositionA - Radius,
		LinePositionB - Radius,
		LinePositionA + Radius,
		LinePositionB + Radius,
	};
	math::vector3f Min = Vertex[0];
	math::vector3f Max = Vertex[0];
	for (size_t index = 1; index < 4; ++index)
	{
		Min = (math::min)(Min, Vertex[index]);
		Max = (math::max)(Max, Vertex[index]);
	}
	min = math::vector3f(Min).to_float3();
	max = math::vector3f(Max).to_float3();
}

void bounding_capsule::on_change_parameter()
{
	bounding_object::on_change_parameter();

	math::quaternionf Rotation(get_rotation());
	math::matrixf Matrix = math::matrixf::rotation_quaternion(Rotation);

	float length = get_length() * 0.5f;
	math::vector3f Line = Matrix.to_vector3(1) * length;
	math::vector3f Position(get_position());
	math::vector3f Start = Position - Line;
	math::vector3f End = Position + Line;
	math::float3 start, end;
	start = Start.to_float3();
	end = End.to_float3();
	line = decltype(line)(line::types::segment, start, end);

}
