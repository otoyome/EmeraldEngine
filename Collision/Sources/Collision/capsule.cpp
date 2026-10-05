#include	"collision/capsule.h"

#include	"collision/sphere.h"
#include	"collision/box.h"
#include	"collision/mesh.h"

using namespace collision;

double bounding_capsule::get_radius() const
{
	return get_local_scale().x;
}

void bounding_capsule::set_radius(double radius)
{
	set_local_scale({std::fabs(radius), get_length(), 0.0f});
}

double bounding_capsule::get_length() const
{
	return get_local_scale().y;
}

void bounding_capsule::set_length(double length)
{
	set_local_scale({get_radius(), std::fabs(length), 0.0f});
}

math::vector3d bounding_capsule::get_axis_vector() const
{
	return math::matrixd::rotation_quaternion(math::quaterniond(get_rotation())).to_vector3(1);
}

math::double3 bounding_capsule::get_axis_float3() const
{
	math::double3 vector;
	vector = math::vector3d(get_axis_vector()).to_double3();
	return vector;
}

bounding_capsule::bounding_capsule()
: bounding_object()
{
	//	線分の再計算
	on_change_parameter();
}

bounding_capsule::bounding_capsule(const math::double3& position, const math::double4& rotation, double radius, double length)
: bounding_object({radius, length, 0.0f}, rotation, position)
{
	//	線分の再計算
	on_change_parameter();
}

bounding_capsule::~bounding_capsule()
{
}

bool bounding_capsule::intersect_point(const math::double3& point) const
{
	math::double3 horizon;
	double delta;
	return line.calculate_distance(horizon, delta, point) - get_radius() <= 0.0f;
}

bool bounding_capsule::intersect_segment(const math::double3& start, const math::double3& end, intersect_data* intersect) const
{
	math::vector3d Start(start);
	math::vector3d End(end);
	math::vector3d Line0(line[0]);
	math::vector3d Line1(line[1]);
	const double radius = get_radius();

	//	軸上の最近点
	auto axis_point = [&](math::vector3d Point)
	{
		math::double3 point, horizon;
		double delta;
		point = Point.to_double3();
		line.calculate_distance(horizon, delta, point);
		return math::vector3d(horizon);
	};

	//	始点が内側
	math::vector3d StartAxis = axis_point(Start);
	math::vector3d StartVector = Start - StartAxis;
	double start_length = StartVector.length();
	if (start_length <= radius)
	{
		if (intersect)
		{
			math::vector3d Normal = (start_length > 1e-6f) ? StartVector * (1.0f / start_length)
															  : (Start - End).normalized();
			math::double3 normal;
			normal = Normal.to_double3();
			intersect->add(start, normal, 0.0f);
		}
		return true;
	}

	//	両端の球と円柱で最も近い交点
	math::vector3d HitPosition;
	double hit_distance = 0.0f;
	double near_distance = FLT_MAX;
	math::vector3d NearPosition = math::vector3d::zero();
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
		math::vector3d Normal = (NearPosition - axis_point(NearPosition)).normalized();
		math::double3 position, normal;
		position = NearPosition.to_double3();
		normal = Normal.to_double3();
		intersect->add(position, normal, near_distance);
	}
	return true;
}

bool bounding_capsule::intersect_sphere(const math::double3& point, double radius, intersect_data* intersect) const
{
	math::double3 horizon;
	double delta;
	double length = line.calculate_distance(horizon, delta, point) - get_radius() - radius;

	if( intersect && length <= 0.0f )
	{
		//	軸上の最近点から自分を押し出す向き
		math::vector3d Point(point);
		math::vector3d Horizon(horizon);
		math::vector3d Vector = Horizon - Point;
		math::vector3d Normal = (Vector.square_length() > 1e-12f)
			? Vector.normalized()
			: math::vector3d::orthogonal(get_axis_vector()).normalized();

		math::double3 insert_point, normal;
		normal = Normal.to_double3();
		insert_point = (Horizon - (Normal * get_radius())).to_double3();
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
	math::double3 horizon1, horizon2;
	double delta1, delta2;
	double length = line.calculate_distance(horizon1, delta1, horizon2, delta2, capsule.line) - (get_radius() + capsule.get_radius());

	if (intersect && length <= 0.0f)
	{
		//	相手から自分へ向かう向き
		math::vector3d Horizon1(horizon1);
		math::vector3d Horizon2(horizon2);
		math::vector3d Vector = Horizon1 - Horizon2;
		math::vector3d Normal;
		if (Vector.square_length() > 1e-12f)
		{
			Normal = Vector.normalized();
		}
		else
		{
			//	軸が交差している場合は両軸に垂直
			Normal = math::vector3d::cross(get_axis_vector(), capsule.get_axis_vector());
			if (Normal.square_length() <= 1e-12f)
				Normal = math::vector3d::orthogonal(get_axis_vector());
			Normal = Normal.normalized();
		}

		math::double3 insert_point, normal;
		normal = Normal.to_double3();
		insert_point = (Horizon1 - (Normal * get_radius())).to_double3();
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

bool bounding_capsule::intersect_sphere_cast(const math::double3& start, const math::double3& end, double radius, intersect_data* intersect) const
{
	math::vector3d Start(start);
	math::vector3d End(end);
	math::vector3d Direction = (End - Start).normalized();

	math::vector3d SphereCenter0(line[0]);
	math::vector3d SphereCenter1(line[1]);
	const double combined_radius = get_radius() + radius;

	//	軸上の最近点
	auto axis_point = [&](math::vector3d Point)
	{
		math::double3 point, horizon;
		double delta;
		point = Point.to_double3();
		line.calculate_distance(horizon, delta, point);
		return math::vector3d(horizon);
	};

	//	開始時の重なり
	{
		math::vector3d AxisPoint = axis_point(Start);
		math::vector3d ToStart = Start - AxisPoint;
		double start_distance = ToStart.length();
		if (start_distance < combined_radius)
		{
			math::vector3d Normal = (start_distance > 1e-6f) ? ToStart * (1.0f / start_distance) : -Direction;
			if (math::vector3d::dot(Direction, Normal) >= 0.0f)
				return false;
			add_cast_hit(intersect, Start, AxisPoint + (Normal * get_radius()), Normal, 0.0f);
			return true;
		}
	}

	math::vector3d NearCenter = math::vector3d::zero(), HitPosition;
	double near_distance = FLT_MAX, hit_distance;
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
		math::vector3d AxisPoint = axis_point(NearCenter);
		math::vector3d Normal = (NearCenter - AxisPoint).normalized();
		math::vector3d Contact = AxisPoint + (Normal * get_radius());
		add_cast_hit(intersect, NearCenter, Contact, Normal, near_distance);
	}
	return true;
}

bool bounding_capsule::intersect_capsule_cast(const math::double3& start, const math::double3& end,
											  double radius, double length, const math::double4& rotation,
											  intersect_data* intersect) const
{
	math::vector3d axis = math::quaterniond(rotation).rotate(math::vector3d(0.0f, 1.0f, 0.0f));
	return intersect_capsule_cast_by_axis_spheres(start, end, radius, length, axis, intersect);
}

void bounding_capsule::include_corner(math::double3& min, math::double3& max) const
{
	math::vector3d LinePositionA(line[0]);
	math::vector3d LinePositionB(line[1]);
	const math::vector3d Radius(get_radius(), get_radius(), get_radius());
	math::vector3d Vertex[4] =
	{
		LinePositionA - Radius,
		LinePositionB - Radius,
		LinePositionA + Radius,
		LinePositionB + Radius,
	};
	math::vector3d Min = Vertex[0];
	math::vector3d Max = Vertex[0];
	for (size_t index = 1; index < 4; ++index)
	{
		Min = (math::min)(Min, Vertex[index]);
		Max = (math::max)(Max, Vertex[index]);
	}
	min = math::vector3d(Min).to_double3();
	max = math::vector3d(Max).to_double3();
}

void bounding_capsule::on_change_parameter()
{
	bounding_object::on_change_parameter();

	math::quaterniond Rotation(get_rotation());
	math::matrixd Matrix = math::matrixd::rotation_quaternion(Rotation);

	double length = get_length() * 0.5f;
	math::vector3d Line = Matrix.to_vector3(1) * length;
	math::vector3d Position(get_position());
	math::vector3d Start = Position - Line;
	math::vector3d End = Position + Line;
	math::double3 start, end;
	start = Start.to_double3();
	end = End.to_double3();
	line = decltype(line)(lined::types::segment, start, end);

}
