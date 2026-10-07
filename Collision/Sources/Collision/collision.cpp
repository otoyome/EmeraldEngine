#include	"collision/collision.h"

#include	"collision/sphere.h"
#include	"collision/capsule.h"
#include	"collision/box.h"
#include	"collision/mesh.h"

#include	<new>

using namespace collision;

void intersect_data::add(const infomation& infomation)
{
	if (count < inline_capacity)
		new (&inline_storage.items[count]) intersect_data::infomation(infomation);
	else
		overflow_infomations.emplace_back(infomation);
	++count;
}

void intersect_data::add(const math::double3& point, const math::double3& normal, double length)
{
	infomation	infomation;
	infomation.point = point;
	infomation.normal = math::vector3d(normal).normalized().to_double3();
	infomation.length = length;
	add(infomation);
}

void intersect_data::add(const math::float3& point, const math::float3& normal, float length)
{
	add(math::double3(point), math::double3(normal), static_cast<double>(length));
}

void intersect_data::clear()
{
	count = 0;
	overflow_infomations.clear();
}

void intersect_data::swap(intersect_data& other) noexcept
{
	const std::size_t this_inline_count = (std::min)(count, inline_capacity);
	const std::size_t other_inline_count = (std::min)(other.count, inline_capacity);
	const std::size_t common_count = (std::min)(this_inline_count, other_inline_count);
	for (std::size_t index = 0; index < common_count; ++index)
		std::swap(inline_storage.items[index], other.inline_storage.items[index]);
	for (std::size_t index = common_count; index < this_inline_count; ++index)
		new (&other.inline_storage.items[index]) infomation(inline_storage.items[index]);
	for (std::size_t index = common_count; index < other_inline_count; ++index)
		new (&inline_storage.items[index]) infomation(other.inline_storage.items[index]);
	overflow_infomations.swap(other.overflow_infomations);
	std::swap(count, other.count);
}

bool intersect_data::multiply_matrix(std::size_t index, const math::double4x4& matrix)
{
	return multiply_matrix(index, math::matrixd(matrix));
}

bool intersect_data::multiply_matrix(std::size_t index, const math::matrixd& matrix)
{
	if (index >= count)
		return false;

	auto& infomation = (*this)[index];
	math::vector3d Position, Position2, Normal;
	Position = math::vector3d::transform_coord(math::vector3d(infomation.point), matrix);
	Position2 = math::vector3d::transform_coord(math::vector3d(infomation.point2), matrix);
	Normal = math::vector3d::transform_normal(math::vector3d(infomation.normal), matrix);
	infomation.point = Position.to_double3();
	infomation.point2 = Position2.to_double3();
	infomation.normal = Normal.normalized().to_double3();
	return true;
}

void intersect_data::multiply_matrices(std::size_t begin, const math::matrixd& matrix)
{
	for (std::size_t index = begin; index < count; ++index)
		multiply_matrix(index, matrix);
}

void intersect_data::flip_normals_from(std::size_t begin)
{
	for (std::size_t index = begin; index < count; ++index)
	{
		auto& normal = (*this)[index].normal;
		normal.x = -normal.x;
		normal.y = -normal.y;
		normal.z = -normal.z;
	}
}

void intersect_data::append(const intersect_data& data)
{
	const std::size_t data_count = data.count;
	for (std::size_t index = 0; index < data_count; ++index)
		add(data[index]);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const math::double3& bounding_object::get_local_scale() const
{
	return	local_scale;
}

void bounding_object::set_local_scale(const math::double3& scale)
{
	this->local_scale = scale;
	on_change_parameter();
}

const math::rigid_transformd& bounding_object::get_transform() const
{
	return transform;
}

void bounding_object::set_transform(const math::rigid_transformd& transform)
{
	this->transform = transform;
	on_change_parameter();
}

const math::quaterniond& bounding_object::get_rotation() const
{
	return	transform.get_rotation();
}

void bounding_object::set_rotation(const math::quaterniond& rotation)
{
	transform.set_rotation(rotation);
	on_change_parameter();
}

const math::vector3d& bounding_object::get_position() const
{
	return transform.get_translation();
}

void bounding_object::set_position(const math::vector3d& position)
{
	transform.set_translation(position);
	on_change_parameter();
}

math::float4x4 bounding_object::get_transform_float4x4() const
{
	return math::float4x4(transform_matrix);
}

math::matrixd bounding_object::get_transform_matrix() const
{
	return	math::matrixd(transform_matrix);
}

math::float4x4 bounding_object::get_inverse_transform_float4x4() const
{
	return math::float4x4(inverse_transform_matrix);
}

math::matrixd bounding_object::get_inverse_transform_matrix() const
{
	return	math::matrixd(inverse_transform_matrix);
}

bounding_object::bounding_object()
: local_scale( 1, 1, 1 )
, transform(math::rigid_transformd::identity())
{
	on_change_parameter();
}

bounding_object::bounding_object(const math::double3& local_scale, const math::double4& rotation, const math::double3& position)
: local_scale(local_scale)
, transform(math::quaterniond(rotation), math::vector3d(position))
{
	on_change_parameter();
}

bounding_object::~bounding_object()
{
}

bool bounding_object::intersection(const bounding_object* object, intersect_data* intersect) const
{
	switch (object->get_type())
	{
		case types::sphere: return intersect_sphere(*static_cast<const bounding_sphere*>(object), intersect);
		case types::capsule: return intersect_capsule(*static_cast<const bounding_capsule*>(object), intersect);
		case types::box: return intersect_box(*static_cast<const bounding_box*>(object), intersect);
		case types::mesh: return intersect_mesh(*static_cast<const bounding_mesh*>(object), intersect);
	}
	assert(!"Not Comaptible Collilsion.");
	return	false;
}

bool bounding_object::intersect_capsule_cast_by_axis_spheres(const math::double3& start, const math::double3& end,
															   double radius, double length, math::vector3d axis,
															   intersect_data* intersect) const
{

	math::vector3d Axis = axis.normalized();
	double half = length * 0.5f;

	int samples = 1;
	if (length > FLT_EPSILON && radius > FLT_EPSILON)
	{
		samples = static_cast<int>(std::ceil(length / radius)) + 1;
		if (samples < 2)
			samples = 2;
	}

	math::vector3d Start(start);
	math::vector3d End(end);

	math::vector3d Move = End - Start;
	double move_length = Move.length();
	math::vector3d MoveDirection = (move_length > FLT_EPSILON) ? Move * (1.0f / move_length) : math::vector3d::zero();

	bool hit = false;
	double nearest_distance = FLT_MAX;
	intersect_data nearest, sample;
	for (int index = 0; index < samples; ++index)
	{
		double s = (samples > 1)
			? (-half + length * (static_cast<double>(index) / static_cast<double>(samples - 1)))
			: 0.0f;
		math::vector3d Offset = Axis * s;

		math::double3 sample_start, sample_end;
		sample_start = (Start + Offset).to_double3();
		sample_end = (End + Offset).to_double3();

		//	開始時の重なりはスフィアキャスト側で判定
		sample.clear();
		if (intersect_sphere_cast(sample_start, sample_end, radius, &sample) && sample.size() > 0)
		{
			double travel = sample[0].length;
			if (travel < nearest_distance)
			{
				nearest_distance = travel;
				nearest.swap(sample);
				nearest[0].length = travel;
				//	接触時のカプセル中心
				nearest[0].point = (Start + (MoveDirection * travel)).to_double3();
				hit = true;
			}
		}
	}

	if (hit && intersect)
		intersect->append(nearest);
	return hit;
}

void bounding_object::on_change_parameter()
{
	transform_matrix = transform.get_transform().to_double4x4();
	inverse_transform_matrix = transform.get_inverse_transform().to_double4x4();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool collision::ray_cast(const math::float3& start, const math::float3& end, const triangles& triangles, intersect_data* result)
{
	math::vector3f Start(start);
	math::vector3f End(end);
	if (triangles.indices_stride_16)
		return ray_cast_16(Start, End, triangles, result);

	return ray_cast_32(Start, End, triangles, result);
}

bool collision::ray_cast(math::vector3f Start, math::vector3f End, const triangles& triangles, intersect_data* result)
{
	if (triangles.indices_stride_16)
		return ray_cast_16(Start, End, triangles, result);

	return ray_cast_32(Start, End, triangles, result);
}

bool collision::ray_cast_16(math::vector3f Start, math::vector3f End, const triangles& triangles, intersect_data* result)
{
	math::vector3f Vec = End - Start;
	math::vector3f Dir = Vec.normalized();
	float Length = Vec.length();

	float neart = Length;
	const std::uint16_t* indices = reinterpret_cast<const std::uint16_t*>(triangles.indices);
	const size_t offsets[] =
	{
		triangles.reverse_index ? 2u : 0u,
		1u,
		triangles.reverse_index ? 0u : 2u,
	};

	bool hit = false;
	math::vector3f HitPosition, HitNormal;
	for (size_t index = triangles.index_start; index + 2 < triangles.index_start + triangles.index_count; index += 3)
	{
		std::uint16_t ai = indices[index + offsets[0]];
		std::uint16_t bi = indices[index + offsets[1]];
		std::uint16_t ci = indices[index + offsets[2]];

		math::vector3f A(*(triangles.positions + ai));
		math::vector3f B(*(triangles.positions + bi));
		math::vector3f C(*(triangles.positions + ci));

		math::vector3f AB = B - A;
		math::vector3f BC = C - B;
		math::vector3f CA = A - C;

		math::vector3f Normal = math::vector3f::cross(AB, BC);
		float Dot = math::vector3f::dot(Normal, Dir);
		if (Dot >= 0)
			continue;

		math::vector3f V = A - Start;
		float T = (math::vector3f::dot(Normal, V)) / Dot;

		float t = T;
		if (t < 0 || t > neart)
			continue;

		math::vector3f Position = (Dir * T) + Start;

		math::vector3f Cross;
		V = A - Position;
		Cross = math::vector3f::cross(V, AB);
		Dot = math::vector3f::dot(Cross, Normal);
		if (Dot < 0)
			continue;

		V = B - Position;
		Cross = math::vector3f::cross(V, BC);
		Dot = math::vector3f::dot(Cross, Normal);
		if (Dot < 0)
			continue;

		V = C - Position;
		Cross = math::vector3f::cross(V, CA);
		Dot = math::vector3f::dot(Cross, Normal);
		if (Dot < 0)
			continue;

		neart = t;
		HitPosition = Position;
		HitNormal = Normal;
		hit = true;
	}
	if (hit && result)
	{
		intersect_data::infomation infomation;
		infomation.point = math::double3(HitPosition.to_float3());
		infomation.normal = math::double3(HitNormal.normalized().to_float3());
		infomation.length = neart;
		result->add(infomation);
	}
	return	hit;
}

bool collision::ray_cast_32(math::vector3f Start, math::vector3f End, const triangles& triangles, intersect_data* result)
{
	math::vector3f Vec = End - Start;
	math::vector3f Dir = Vec.normalized();
	float Length = Vec.length();

	float neart = Length;
	const std::uint32_t* indices = reinterpret_cast<const std::uint32_t*>(triangles.indices);
	const std::uint32_t offsets[] =
	{
		triangles.reverse_index ? 2u : 0u,
		1u,
		triangles.reverse_index ? 0u : 2u,
	};

	bool hit = false;
	math::vector3f HitPosition, HitNormal;
	for (size_t index = triangles.index_start; index + 2 < triangles.index_start + triangles.index_count; index += 3)
	{
		std::uint32_t ai = indices[index + offsets[0]];
		std::uint32_t bi = indices[index + offsets[1]];
		std::uint32_t ci = indices[index + offsets[2]];

		math::vector3f A(*(triangles.positions + ai));
		math::vector3f B(*(triangles.positions + bi));
		math::vector3f C(*(triangles.positions + ci));

		math::vector3f AB = B - A;
		math::vector3f BC = C - B;
		math::vector3f CA = A - C;

		math::vector3f Normal = math::vector3f::cross(AB, BC);
		float Dot = math::vector3f::dot(Normal, Dir);
		if (Dot >= 0)
			continue;

		math::vector3f V = A - Start;
		float T = (math::vector3f::dot(Normal, V)) / Dot;

		float t = T;
		if (t < 0 || t > neart)
			continue;

		math::vector3f Position = (Dir * T) + Start;

		math::vector3f Cross;
		V = A - Position;
		Cross = math::vector3f::cross(V, AB);
		Dot = math::vector3f::dot(Cross, Normal);
		if (Dot < 0)
			continue;

		V = B - Position;
		Cross = math::vector3f::cross(V, BC);
		Dot = math::vector3f::dot(Cross, Normal);
		if (Dot < 0)
			continue;

		V = C - Position;
		Cross = math::vector3f::cross(V, CA);
		Dot = math::vector3f::dot(Cross, Normal);
		if (Dot < 0)
			continue;

		neart = t;
		HitPosition = Position;
		HitNormal = Normal;
		hit = true;
	}
	if (hit && result)
	{
		intersect_data::infomation infomation;
		infomation.point = math::double3(HitPosition.to_float3());
		infomation.normal = math::double3(HitNormal.normalized().to_float3());
		infomation.length = neart;
		result->add(infomation);
	}
	return	hit;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool collision::intersect_ray_vs_sphere(math::vector3d start, math::vector3d end,
													math::vector3d sphere_pos, double radius,
													math::vector3d& hit_pos, double& hit_distance)
{
	math::vector3d direction = end - start;
	math::vector3d direction_normalize = direction.normalized();
	math::vector3d ray2sphere = sphere_pos - start;
	double projection = math::vector3d::dot(ray2sphere, direction_normalize);

	double dist_sq = ray2sphere.square_length() - projection * projection;
	if (dist_sq < radius * radius)
	{
		hit_distance = projection - std::sqrt(radius * radius - dist_sq);
		if (hit_distance > 0.0f)
		{
			if (hit_distance < direction.length())
			{
				hit_pos = start + (direction_normalize * hit_distance);
				return true;
			}
		}
	}

	hit_distance = 0.0f;
	hit_pos = math::vector3d::zero();
	return false;
}

bool collision::intersect_ray_vs_cylinder(math::vector3d start, math::vector3d end,
													  math::vector3d start_cylinder, math::vector3d end_cylinder,
													  double radius, math::vector3d& hit_pos, double& hit_distance)
{
	math::vector3d d = end_cylinder - start_cylinder;
	math::vector3d m = start - start_cylinder;
	math::vector3d n = end - start;

	double md = math::vector3d::dot(m, d);
	double nd = math::vector3d::dot(n, d);
	double dd = math::vector3d::dot(d, d);

	//	線分が円柱のどちらかの底面に対して完全に外側にあるかどうかを判定
	if (md < 0.0f && md + nd < 0.0f)
		return false;	//	線分が円柱のstart_cylinderの外側にある

	if (md > dd && md + nd > dd)
		return false;	//	線分が円柱のend_cylinderの外側にある

	double nn = math::vector3d::dot(n, n);
	double a = dd * nn - nd * nd;
	double k = math::vector3d::dot(m, m) - radius * radius;
	double c = dd * k - md * md;

	//	線分が円柱の軸に対して平行
	if (std::fabs(a) < 0.0001f)
	{
		if (c > 0.0f)
			return false;	//	線分は円柱の外側

		double t = 0.0f;
		if (md < 0.0f)
		{
			//	start_cylinder側の底面より手前から始まっている
			if (nd <= 0.0f)
				return false;	//	底面に向かって進んでいない（ここには来ないはず）
			t = -md / nd;
		}
		else if (md > dd)
		{
			//	end_cylinder側の底面より先から始まっている
			if (nd >= 0.0f)
				return false;	//	底面に向かって進んでいない（ここには来ないはず）
			t = (dd - md) / nd;
		}

		//	交点が線分の範囲外
		if (t < 0.0f || t > 1.0f)
			return false;

		hit_pos = start + (n * t);
		hit_distance = t * n.length();
		return true;
	}

	//	線分が円柱の軸に対して平行でない
	//	円柱の表面を表す陰関数方程式と直線の方程式の解を求めて交差判定を行う。
	double mn = math::vector3d::dot(m, n);
	double b = dd * mn - nd * md;
	double D = b * b - a * c;	//	判別式

	if (D < 0.0f)
		return false;	//	実数解がないので交差していない

	hit_distance = (-b - std::sqrt(D)) / a;
	if (hit_distance < 0.0f)
	{
		hit_distance = (-b + std::sqrt(D)) / a;
		if (hit_distance < 0.0f)
			return false;	//	交点が線分の外側にあり交差していない
	}
	else if (hit_distance > 1.0f)
	{
		hit_distance = (-b + std::sqrt(D)) / a;
		if (hit_distance > 1.0f)
			return false;	//	交点が線分の外側にあり交差していない
	}

	if (md + hit_distance * nd < 0.0f)
	{
		//	円柱のstart_cylinder側の底面の外で交差
		double t = -md / nd;
		if (k + t * (2.0f * mn + t * nn) > 0.0f)
			return false;
		hit_distance = t;
	}
	else if (md + hit_distance * nd > dd)
	{
		//	円柱のend_cylinder側の底面の外で交差
		double t = (dd - md) / nd;
		if (k + dd - 2.0f * md + t * (2.0f * (mn - nd) + t * nn) > 0.0f)
			return false;
		hit_distance = t;
	}

	//	線分が円柱の底面と底面の間で交差している
	if (hit_distance >= 0.0f && hit_distance <= 1.0f)
	{
		hit_pos = start + (n * hit_distance);
		hit_distance *= n.length();
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void collision::add_cast_hit(intersect_data* intersect, math::vector3d center, math::vector3d contact, math::vector3d normal, double length)
{
	if (!intersect)
		return;

	intersect_data::infomation infomation;
	infomation.point = math::vector3d(center).to_double3();
	infomation.point2 = math::vector3d(contact).to_double3();
	infomation.normal = normal.normalized().to_double3();
	infomation.length = length;
	intersect->add(infomation);
}

collision::cast_start_overlap collision::check_cast_start_overlap(math::vector3d start, math::vector3d end, math::vector3d nearest_point,
																			  double radius, math::vector3d fallback_normal, intersect_data* intersect)
{
	math::vector3d Vector = start - nearest_point;
	double distance_sq = Vector.square_length();
	if (distance_sq >= radius * radius)
		return cast_start_overlap::none;

	double distance = std::sqrt(distance_sq);
	math::vector3d Normal = (distance > 1e-6f) ? Vector * (1.0f / distance) : fallback_normal.normalized();

	math::vector3d Move = end - start;
	if (math::vector3d::dot(Move, Normal) >= 0.0f)
		return cast_start_overlap::leaving;

	add_cast_hit(intersect, start, nearest_point, Normal, 0.0f);
	return cast_start_overlap::hit;
}
