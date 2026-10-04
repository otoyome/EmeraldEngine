#include	"collision/collision.h"

#include	"collision/sphere.h"
#include	"collision/capsule.h"
#include	"collision/box.h"
#include	"collision/mesh.h"

using namespace collision;

intersect_data::intersect_data()
{
}

intersect_data::~intersect_data()
{
}

void intersect_data::add(const infomation& infomation)
{
	infomations.emplace_back(infomation);
}

void intersect_data::add(const math::float3& point, const math::float3& normal, float length)
{
	infomation	infomation;
	infomation.point = point;
	infomation.normal = math::vector3f(normal).normalized().to_float3();
	infomation.length = length;
	add(infomation);
}

void intersect_data::clear()
{
	decltype(infomations)().swap(infomations);
}

bool intersect_data::multiply_matrix(std::size_t index, math::float4x4 matrix)
{
	return multiply_matrix(index, math::matrixf(matrix));
}

bool intersect_data::multiply_matrix(std::size_t index, math::matrixf matrix)
{
	if (index >= infomations.size())
		return false;

	math::vector3f Position, Position2, Normal;
	Position = math::vector3f::transform_coord(math::vector3f(infomations[index].point), matrix);
	Position2 = math::vector3f::transform_coord(math::vector3f(infomations[index].point2), matrix);
	Normal = math::vector3f::transform_normal(math::vector3f(infomations[index].normal), matrix);
	infomations[index].point = Position.to_float3();
	infomations[index].point2 = Position2.to_float3();
	infomations[index].normal = Normal.normalized().to_float3();
	return true;
}

void intersect_data::multiply_matrices(std::size_t begin, math::matrixf matrix)
{
	for (std::size_t index = begin; index < infomations.size(); ++index)
		multiply_matrix(index, matrix);
}

void intersect_data::flip_normals_from(std::size_t begin)
{
	for (std::size_t index = begin; index < infomations.size(); ++index)
	{
		infomations[index].normal.x = -infomations[index].normal.x;
		infomations[index].normal.y = -infomations[index].normal.y;
		infomations[index].normal.z = -infomations[index].normal.z;
	}
}

void intersect_data::append(const intersect_data& data)
{
	infomations.insert(infomations.end(), data.infomations.begin(), data.infomations.end());
}

std::size_t intersect_data::size() const
{
	return infomations.size();
}

const intersect_data::infomation& intersect_data::operator[](std::size_t index) const
{
	return infomations[index];
}

intersect_data::infomation& intersect_data::operator[](std::size_t index)
{
	return infomations[index];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const math::float3& bounding_object::get_local_scale() const
{
	return	local_scale;
}

void bounding_object::set_local_scale(const math::float3& scale)
{
	this->local_scale = scale;
	on_change_parameter();
}

const math::float4& bounding_object::get_rotation() const
{
	return	rotation;
}

void bounding_object::set_rotation(const math::float4& rotation)
{
	this->rotation = rotation;
	on_change_parameter();
}

const math::float3& bounding_object::get_position() const
{
	return position;
}

void bounding_object::set_position(const math::float3& position)
{
	this->position	= position;
	on_change_parameter();
}

math::float4x4 bounding_object::get_transform_float4x4() const
{
	return transform_matrix;
}

math::matrixf bounding_object::get_transform_matrix() const
{
	return	math::matrixf(transform_matrix);
}

math::float4x4 bounding_object::get_inverse_transform_float4x4() const
{
	return inverse_transform_matrix;
}

math::matrixf bounding_object::get_inverse_transform_matrix() const
{
	return	math::matrixf(inverse_transform_matrix);
}

bounding_object::bounding_object()
: local_scale( 1, 1, 1 )
, rotation( 0, 0, 0, 1 )
, position( 0, 0, 0 )
{
	on_change_parameter();
}

bounding_object::bounding_object(const math::float3& local_scale, const math::float4& rotation, const math::float3& position)
: local_scale(local_scale)
, rotation(rotation)
, position(position)
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

bool bounding_object::intersect_capsule_cast_by_axis_spheres(const math::float3& start, const math::float3& end,
															   float radius, float length, math::vector3f axis,
															   intersect_data* intersect) const
{

	math::vector3f Axis = axis.normalized();
	float half = length * 0.5f;

	int samples = 1;
	if (length > FLT_EPSILON && radius > FLT_EPSILON)
	{
		samples = static_cast<int>(ceilf(length / radius)) + 1;
		if (samples < 2)
			samples = 2;
	}

	math::vector3f Start(start);
	math::vector3f End(end);

	math::vector3f Move = End - Start;
	float move_length = Move.length();
	math::vector3f MoveDirection = (move_length > FLT_EPSILON) ? Move * (1.0f / move_length) : math::vector3f::zero();

	bool hit = false;
	float nearest_distance = FLT_MAX;
	intersect_data nearest;
	for (int index = 0; index < samples; ++index)
	{
		float s = (samples > 1)
			? (-half + length * (static_cast<float>(index) / static_cast<float>(samples - 1)))
			: 0.0f;
		math::vector3f Offset = Axis * s;

		math::float3 sample_start, sample_end;
		sample_start = (Start + Offset).to_float3();
		sample_end = (End + Offset).to_float3();

		//	開始時の重なりはスフィアキャスト側で判定
		intersect_data sample;
		if (intersect_sphere_cast(sample_start, sample_end, radius, &sample) && sample.size() > 0)
		{
			float travel = sample[0].length;
			if (travel < nearest_distance)
			{
				nearest_distance = travel;
				nearest = sample;
				nearest[0].length = travel;
				//	接触時のカプセル中心
				nearest[0].point = (Start + (MoveDirection * travel)).to_float3();
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
	math::matrixf Rotation = math::matrixf::rotation_quaternion(math::quaternionf(rotation));
	math::matrixf Translation = math::matrixf::translation(position.x, position.y, position.z);

	math::matrixf TransformMatrix = Rotation * Translation;
	transform_matrix = TransformMatrix.to_float4x4();
	inverse_transform_matrix = TransformMatrix.inverted().to_float4x4();
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
		infomation.point = HitPosition.to_float3();
		infomation.normal = HitNormal.normalized().to_float3();
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
		infomation.point = HitPosition.to_float3();
		infomation.normal = HitNormal.normalized().to_float3();
		infomation.length = neart;
		result->add(infomation);
	}
	return	hit;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool collision::intersect_ray_vs_sphere(math::vector3f start, math::vector3f end,
													math::vector3f sphere_pos, float radius,
													math::vector3f& hit_pos, float& hit_distance)
{
	math::vector3f direction = end - start;
	math::vector3f direction_normalize = direction.normalized();
	math::vector3f ray2sphere = sphere_pos - start;
	float projection = math::vector3f::dot(ray2sphere, direction_normalize);

	float dist_sq = ray2sphere.square_length() - projection * projection;
	if (dist_sq < radius * radius)
	{
		hit_distance = projection - sqrtf(radius * radius - dist_sq);
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
	hit_pos = math::vector3f::zero();
	return false;
}

bool collision::intersect_ray_vs_cylinder(math::vector3f start, math::vector3f end,
													  math::vector3f start_cylinder, math::vector3f end_cylinder,
													  float radius, math::vector3f& hit_pos, float& hit_distance)
{
	math::vector3f d = end_cylinder - start_cylinder;
	math::vector3f m = start - start_cylinder;
	math::vector3f n = end - start;

	float md = math::vector3f::dot(m, d);
	float nd = math::vector3f::dot(n, d);
	float dd = math::vector3f::dot(d, d);

	//	線分が円柱のどちらかの底面に対して完全に外側にあるかどうかを判定
	if (md < 0.0f && md + nd < 0.0f)
		return false;	//	線分が円柱のstart_cylinderの外側にある

	if (md > dd && md + nd > dd)
		return false;	//	線分が円柱のend_cylinderの外側にある

	float nn = math::vector3f::dot(n, n);
	float a = dd * nn - nd * nd;
	float k = math::vector3f::dot(m, m) - radius * radius;
	float c = dd * k - md * md;

	//	線分が円柱の軸に対して平行
	if (fabsf(a) < 0.0001f)
	{
		if (c > 0.0f)
			return false;	//	線分は円柱の外側

		float t = 0.0f;
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
	float mn = math::vector3f::dot(m, n);
	float b = dd * mn - nd * md;
	float D = b * b - a * c;	//	判別式

	if (D < 0.0f)
		return false;	//	実数解がないので交差していない

	hit_distance = (-b - sqrtf(D)) / a;
	if (hit_distance < 0.0f)
	{
		hit_distance = (-b + sqrtf(D)) / a;
		if (hit_distance < 0.0f)
			return false;	//	交点が線分の外側にあり交差していない
	}
	else if (hit_distance > 1.0f)
	{
		hit_distance = (-b + sqrtf(D)) / a;
		if (hit_distance > 1.0f)
			return false;	//	交点が線分の外側にあり交差していない
	}

	if (md + hit_distance * nd < 0.0f)
	{
		//	円柱のstart_cylinder側の底面の外で交差
		float t = -md / nd;
		if (k + t * (2.0f * mn + t * nn) > 0.0f)
			return false;
		hit_distance = t;
	}
	else if (md + hit_distance * nd > dd)
	{
		//	円柱のend_cylinder側の底面の外で交差
		float t = (dd - md) / nd;
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
void collision::add_cast_hit(intersect_data* intersect, math::vector3f center, math::vector3f contact, math::vector3f normal, float length)
{
	if (!intersect)
		return;

	intersect_data::infomation infomation;
	infomation.point = math::vector3f(center).to_float3();
	infomation.point2 = math::vector3f(contact).to_float3();
	infomation.normal = normal.normalized().to_float3();
	infomation.length = length;
	intersect->add(infomation);
}

collision::cast_start_overlap collision::check_cast_start_overlap(math::vector3f start, math::vector3f end, math::vector3f nearest_point,
																			  float radius, math::vector3f fallback_normal, intersect_data* intersect)
{
	math::vector3f Vector = start - nearest_point;
	float distance_sq = Vector.square_length();
	if (distance_sq >= radius * radius)
		return cast_start_overlap::none;

	float distance = sqrtf(distance_sq);
	math::vector3f Normal = (distance > 1e-6f) ? Vector * (1.0f / distance) : fallback_normal.normalized();

	math::vector3f Move = end - start;
	if (math::vector3f::dot(Move, Normal) >= 0.0f)
		return cast_start_overlap::leaving;

	add_cast_hit(intersect, start, nearest_point, Normal, 0.0f);
	return cast_start_overlap::hit;
}
