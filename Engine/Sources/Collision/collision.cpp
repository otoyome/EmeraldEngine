#include	"collision/collision.h"

#include	"collision/sphere.h"
#include	"collision/capsule.h"
#include	"collision/box.h"

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

void intersect_data::add(const DirectX::XMFLOAT3& point, const DirectX::XMFLOAT3& normal, float length)
{
	infomation	infomation;
	infomation.point = point;
	DirectX::XMStoreFloat3(&infomation.normal, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&normal)));
	infomation.length = length;
	add(infomation);
}

void intersect_data::clear()
{
	system_utility::std_vector_swap_clear(infomations);
}

bool intersect_data::multiply_matrix(std::size_t index, DirectX::XMFLOAT4X4 matrix)
{
	return multiply_matrix(index, DirectX::XMLoadFloat4x4(&matrix));
}

bool intersect_data::multiply_matrix(std::size_t index, DirectX::XMMATRIX matrix)
{
	if (index >= infomations.size())
		return false;

	DirectX::XMVECTOR Position, Normal;
	Position = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&infomations[index].point), matrix);
	Normal = DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&infomations[index].normal), matrix);
	DirectX::XMStoreFloat3(&infomations[index].point, Position);
	DirectX::XMStoreFloat3(&infomations[index].normal, DirectX::XMVector3Normalize(Normal));
	return true;
}

std::size_t intersect_data::size()
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
const DirectX::XMFLOAT3& bounding_object::get_local_scale() const
{
	return	local_scale;
}

void bounding_object::set_local_scale(const DirectX::XMFLOAT3& scale)
{
	this->local_scale = scale;
	on_change_parameter();
}

const DirectX::XMFLOAT4& bounding_object::get_rotation() const
{
	return	rotation;
}

void bounding_object::set_rotation(const DirectX::XMFLOAT4& rotation)
{
	this->rotation = rotation;
	on_change_parameter();
}

const DirectX::XMFLOAT3& bounding_object::get_position() const
{
	return position;
}

void bounding_object::set_position(const DirectX::XMFLOAT3& position)
{
	this->position	= position;
	on_change_parameter();
}

DirectX::XMFLOAT4X4 bounding_object::get_transform_float4x4() const
{
	return transform_matrix;
}

DirectX::XMMATRIX bounding_object::get_transform_matrix() const
{
	return	DirectX::XMLoadFloat4x4(&transform_matrix);
}

DirectX::XMFLOAT4X4 bounding_object::get_inverse_transform_float4x4() const
{
	return inverse_transform_matrix;
}

DirectX::XMMATRIX bounding_object::get_inverse_transform_matrix() const
{
	return	DirectX::XMLoadFloat4x4(&inverse_transform_matrix);
}

bounding_object::bounding_object()
: local_scale( 1, 1, 1 )
, rotation( 0, 0, 0, 1 )
, position( 0, 0, 0 )
{
	on_change_parameter();
}

bounding_object::bounding_object(const DirectX::XMFLOAT3& local_scale, const DirectX::XMFLOAT4& rotation, const DirectX::XMFLOAT3& position)
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
		case types::mesh: return intersect_mesh(*reinterpret_cast<const bounding_mesh*>(object), intersect);
	}
	assert(!"Not Comaptible Collilsion.");
	return	false;
}

bool bounding_object::intersect_capsule_cast_by_axis_spheres(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
															   float radius, float length, DirectX::XMVECTOR axis,
															   intersect_data* intersect) const
{
	using namespace DirectX;

	XMVECTOR Axis = XMVector3Normalize(axis);
	float half = length * 0.5f;

	int samples = 1;
	if (length > FLT_EPSILON && radius > FLT_EPSILON)
	{
		samples = static_cast<int>(ceilf(length / radius)) + 1;
		if (samples < 2)
			samples = 2;
	}

	XMVECTOR Start = XMLoadFloat3(&start);
	XMVECTOR End = XMLoadFloat3(&end);

	bool hit = false;
	float nearest_distance = FLT_MAX;
	intersect_data nearest;
	for (int index = 0; index < samples; ++index)
	{
		float s = (samples > 1)
			? (-half + length * (static_cast<float>(index) / static_cast<float>(samples - 1)))
			: 0.0f;
		XMVECTOR Offset = XMVectorScale(Axis, s);

		DirectX::XMFLOAT3 sample_start, sample_end;
		XMStoreFloat3(&sample_start, XMVectorAdd(Start, Offset));
		XMStoreFloat3(&sample_end, XMVectorAdd(End, Offset));

		intersect_data sample;
		if (intersect_sphere_cast(sample_start, sample_end, radius, &sample) && sample.size() > 0)
		{
			if (sample[0].length < nearest_distance)
			{
				nearest_distance = sample[0].length;
				nearest = sample;
				hit = true;
			}
		}
	}

	if (hit && intersect)
		*intersect = nearest;
	return hit;
}

void bounding_object::on_change_parameter()
{
	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation));
	DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(position.x, position.y, position.z);

	DirectX::XMMATRIX TransformMatrix = Rotation * Translation;
	DirectX::XMStoreFloat4x4(&transform_matrix, TransformMatrix);
	DirectX::XMStoreFloat4x4(&inverse_transform_matrix, DirectX::XMMatrixInverse(nullptr, TransformMatrix));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool collision::ray_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const triangles& triangles, intersect_data* result)
{
	DirectX::XMVECTOR Start	= DirectX::XMLoadFloat3(&start);
	DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);
	if (triangles.indices_stride_16)
		return ray_cast_16(Start, End, triangles, result);

	return ray_cast_32(Start, End, triangles, result);
}

bool collision::ray_cast(DirectX::XMVECTOR Start, DirectX::XMVECTOR End, const triangles& triangles, intersect_data* result)
{
	if (triangles.indices_stride_16)
		return ray_cast_16(Start, End, triangles, result);

	return ray_cast_32(Start, End, triangles, result);
}

bool collision::ray_cast_16(DirectX::XMVECTOR Start, DirectX::XMVECTOR End, const triangles& triangles, intersect_data* result)
{
	DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(End, Start);
	DirectX::XMVECTOR Dir = DirectX::XMVector3Normalize(Vec);
	DirectX::XMVECTOR Length = DirectX::XMVector3Length(Vec);

	float neart = DirectX::XMVectorGetX(Length);
	const USHORT* indices = reinterpret_cast<const USHORT*>(triangles.indices);
	const size_t offsets[] =
	{
		triangles.reverse_index ? 0u : 2u,
		1u,
		triangles.reverse_index ? 2u : 0u,
	};

	bool hit = false;
	DirectX::XMVECTOR HitPosition, HitNormal;
	for (size_t index = triangles.index_start; index < triangles.index_count; index += 3)
	{
		USHORT ai = indices[index + offsets[0]];
		USHORT bi = indices[index + offsets[1]];
		USHORT ci = indices[index + offsets[2]];

		DirectX::XMVECTOR A = DirectX::XMLoadFloat3(triangles.positions + ai);
		DirectX::XMVECTOR B = DirectX::XMLoadFloat3(triangles.positions + bi);
		DirectX::XMVECTOR C = DirectX::XMLoadFloat3(triangles.positions + ci);

		DirectX::XMVECTOR AB = DirectX::XMVectorSubtract(B, A);
		DirectX::XMVECTOR BC = DirectX::XMVectorSubtract(C, B);
		DirectX::XMVECTOR CA = DirectX::XMVectorSubtract(A, C);

		DirectX::XMVECTOR Normal = DirectX::XMVector3Cross(AB, BC);
		DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(Normal, Dir);
		if (DirectX::XMVectorGetX(Dot) >= 0)
			continue;

		DirectX::XMVECTOR V = DirectX::XMVectorSubtract(A, Start);
		DirectX::XMVECTOR T = DirectX::XMVectorDivide( DirectX::XMVector3Dot(Normal, V), Dot);

		float t = DirectX::XMVectorGetX(T);
		if (t < 0 || t > neart)
			continue;

		DirectX::XMVECTOR Position = DirectX::XMVectorAdd(DirectX::XMVectorMultiply(Dir, T), Start);

		DirectX::XMVECTOR Cross;
		V = DirectX::XMVectorSubtract(A, Position);
		Cross = DirectX::XMVector3Cross(V, AB);
		Dot = DirectX::XMVector3Dot(Cross, Normal);
		if (DirectX::XMVectorGetX(Dot) < 0)
			continue;

		V = DirectX::XMVectorSubtract(B, Position);
		Cross = DirectX::XMVector3Cross(V, BC);
		Dot = DirectX::XMVector3Dot(Cross, Normal);
		if (DirectX::XMVectorGetX(Dot) < 0)
			continue;

		V = DirectX::XMVectorSubtract(C, Position);
		Cross = DirectX::XMVector3Cross(V, CA);
		Dot = DirectX::XMVector3Dot(Cross, Normal);
		if (DirectX::XMVectorGetX(Dot) < 0)
			continue;

		neart = t;
		HitPosition = Position;
		HitNormal = Normal;
		hit = true;
	}
	if (hit && result)
	{
		intersect_data::infomation infomation;
		DirectX::XMStoreFloat3(&infomation.point, HitPosition);
		DirectX::XMStoreFloat3(&infomation.normal, HitNormal);
		infomation.length = neart;
		result->add(infomation);
	}
	return	hit;
}

bool collision::ray_cast_32(DirectX::XMVECTOR Start, DirectX::XMVECTOR End, const triangles& triangles, intersect_data* result)
{
	DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(End, Start);
	DirectX::XMVECTOR Dir = DirectX::XMVector3Normalize(Vec);
	DirectX::XMVECTOR Length = DirectX::XMVector3Length(Vec);

	float neart = DirectX::XMVectorGetX(Length);
	const UINT* indices = reinterpret_cast<const UINT*>(triangles.indices);
	const UINT offsets[] =
	{
		triangles.reverse_index ? 2u : 0u,
		1u,
		triangles.reverse_index ? 0u : 2u,
	};

	bool hit = false;
	DirectX::XMVECTOR HitPosition, HitNormal;
	for (size_t index = triangles.index_start; index < triangles.index_count; index += 3)
	{
		UINT ai = indices[index + offsets[0]];
		UINT bi = indices[index + offsets[1]];
		UINT ci = indices[index + offsets[2]];

		DirectX::XMVECTOR A = DirectX::XMLoadFloat3(triangles.positions + ai);
		DirectX::XMVECTOR B = DirectX::XMLoadFloat3(triangles.positions + bi);
		DirectX::XMVECTOR C = DirectX::XMLoadFloat3(triangles.positions + ci);

		DirectX::XMVECTOR AB = DirectX::XMVectorSubtract(B, A);
		DirectX::XMVECTOR BC = DirectX::XMVectorSubtract(C, B);
		DirectX::XMVECTOR CA = DirectX::XMVectorSubtract(A, C);

		DirectX::XMVECTOR Normal = DirectX::XMVector3Cross(AB, BC);
		DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(Normal, Dir);
		if (DirectX::XMVectorGetX(Dot) >= 0)
			continue;

		DirectX::XMVECTOR V = DirectX::XMVectorSubtract(A, Start);
		DirectX::XMVECTOR T = DirectX::XMVectorDivide( DirectX::XMVector3Dot(Normal, V), Dot);

		float t = DirectX::XMVectorGetX(T);
		if (t < 0 || t > neart)
			continue;

		DirectX::XMVECTOR Position = DirectX::XMVectorAdd(DirectX::XMVectorMultiply(Dir, T), Start);

		DirectX::XMVECTOR Cross;
		V = DirectX::XMVectorSubtract(A, Position);
		Cross = DirectX::XMVector3Cross(V, AB);
		Dot = DirectX::XMVector3Dot(Cross, Normal);
		if (DirectX::XMVectorGetX(Dot) < 0)
			continue;

		V = DirectX::XMVectorSubtract(B, Position);
		Cross = DirectX::XMVector3Cross(V, BC);
		Dot = DirectX::XMVector3Dot(Cross, Normal);
		if (DirectX::XMVectorGetX(Dot) < 0)
			continue;

		V = DirectX::XMVectorSubtract(C, Position);
		Cross = DirectX::XMVector3Cross(V, CA);
		Dot = DirectX::XMVector3Dot(Cross, Normal);
		if (DirectX::XMVectorGetX(Dot) < 0)
			continue;

		neart = t;
		HitPosition = Position;
		HitNormal = Normal;
		hit = true;
	}
	if (hit && result)
	{
		intersect_data::infomation infomation;
		DirectX::XMStoreFloat3(&infomation.point, HitPosition);
		DirectX::XMStoreFloat3(&infomation.normal, HitNormal);
		infomation.length = neart;
		result->add(infomation);
	}
	return	hit;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool XM_CALLCONV collision::intersect_ray_vs_sphere(DirectX::XMVECTOR start, DirectX::XMVECTOR end,
													DirectX::XMVECTOR sphere_pos, float radius,
													DirectX::XMVECTOR& hit_pos, float& hit_distance)
{
	DirectX::XMVECTOR direction = DirectX::XMVectorSubtract(end, start);
	DirectX::XMVECTOR direction_normalize = DirectX::XMVector3Normalize(direction);
	DirectX::XMVECTOR ray2sphere = DirectX::XMVectorSubtract(sphere_pos, start);
	float projection = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ray2sphere, direction_normalize));

	float dist_sq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(ray2sphere)) - projection * projection;
	if (dist_sq < radius * radius)
	{
		hit_distance = projection - sqrtf(radius * radius - dist_sq);
		if (hit_distance > 0.0f)
		{
			if (hit_distance < DirectX::XMVectorGetX(DirectX::XMVector3Length(direction)))
			{
				hit_pos = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(direction_normalize, hit_distance));
				return true;
			}
		}
	}

	hit_distance = 0.0f;
	hit_pos = DirectX::XMVectorZero();
	return false;
}

bool XM_CALLCONV collision::intersect_ray_vs_cylinder(DirectX::XMVECTOR start, DirectX::XMVECTOR end,
													  DirectX::XMVECTOR start_cylinder, DirectX::XMVECTOR end_cylinder,
													  float radius, DirectX::XMVECTOR& hit_pos, float& hit_distance)
{
	DirectX::XMVECTOR d = DirectX::XMVectorSubtract(end_cylinder, start_cylinder);
	DirectX::XMVECTOR m = DirectX::XMVectorSubtract(start, start_cylinder);
	DirectX::XMVECTOR n = DirectX::XMVectorSubtract(end, start);

	float md = DirectX::XMVectorGetX(DirectX::XMVector3Dot(m, d));
	float nd = DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, d));
	float dd = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, d));

	//	線分が円柱のどちらかの底面に対して完全に外側にあるかどうかを判定
	if (md < 0.0f && md + nd < 0.0f)
		return false;	//	線分が円柱のstart_cylinderの外側にある

	if (md > dd && md + nd > dd)
		return false;	//	線分が円柱のend_cylinderの外側にある

	float nn = DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, n));
	float a = dd * nn - nd * nd;
	float k = DirectX::XMVectorGetX(DirectX::XMVector3Dot(m, m)) - radius * radius;
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

		hit_pos = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(n, t));
		hit_distance = t * DirectX::XMVectorGetX(DirectX::XMVector3Length(n));
		return true;
	}

	//	線分が円柱の軸に対して平行でない
	//	円柱の表面を表す陰関数方程式と直線の方程式の解を求めて交差判定を行う。
	float mn = DirectX::XMVectorGetX(DirectX::XMVector3Dot(m, n));
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
		hit_pos = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(n, hit_distance));
		hit_distance *= DirectX::XMVectorGetX(DirectX::XMVector3Length(n));
		return true;
	}

	return false;
}
