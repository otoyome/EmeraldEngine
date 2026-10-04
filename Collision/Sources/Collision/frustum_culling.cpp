#include	"collision/frustum_culling.h"

#include	"collision/sphere.h"
#include	"collision/capsule.h"
#include	"collision/box.h"

using namespace collision;

frustum_culling::frustum_culling()
{
}

frustum_culling::frustum_culling(const math::float4x4& projection_matrix)
{
	create_planes(projection_matrix);
}

frustum_culling::~frustum_culling()
{
}

bool frustum_culling::intersection(const bounding_object* object) const
{
	switch (object->get_type())
	{
		case bounding_object::types::sphere: return to_sphere(*static_cast<const bounding_sphere*>(object));
		case bounding_object::types::capsule: return to_capsule(*static_cast<const bounding_capsule*>(object));
		case bounding_object::types::box: return to_box(*static_cast<const bounding_box*>(object));
		case bounding_object::types::mesh:
		{
			//	メッシュはAABBで判定
			math::double3 min, max;
			object->include_corner(min, max);
			return to_aabb(math::float3(min), math::float3(max));
		}
	}
	assert(!"Not Comaptible Collilsion.");
	return false;
}

bool frustum_culling::to_point(const math::float3& point) const
{
	for (size_t index = 0; index < surface_max; ++index)
	{
		if (planes[index].calculate_distance(point) < 0.0f)
		{
			return false;
		}
	}
	return true;
}

bool frustum_culling::to_sphere(const math::float3& point, float radius) const
{
	for (size_t index = 0; index < surface_max; ++index)
	{
		if (planes[index].calculate_distance(point) < -radius)
		{
			return false;
		}
	}
	return true;
}

bool frustum_culling::to_sphere(const bounding_sphere& sphere) const
{
    return	to_sphere(math::float3(sphere.get_position()), static_cast<float>(sphere.get_radius()));
}

bool frustum_culling::to_capsule(const bounding_capsule& capsule) const
{
	//	ただのAABBでの判定
	math::double3 min, max;
	capsule.include_corner(min, max);
	return to_aabb(math::float3(min), math::float3(max));
}

bool frustum_culling::to_box(const bounding_box& box) const
{
	//	ただのAABBでの判定
	math::double3 min, max;
	box.include_corner(min, max);
	return to_aabb(math::float3(min), math::float3(max));
}

bool frustum_culling::to_aabb(const math::float3& min, const math::float3& max) const
{
	int r = 0;
	for (int index = 0; index < surface_max; ++index)
	{
		r = 0;
		r += planes[index].calculate_distance({min.x, min.y, min.z}) <= 0 ? 1 : 0;
		r += planes[index].calculate_distance({max.x, min.y, min.z}) <= 0 ? 1 : 0;
		r += planes[index].calculate_distance({min.x, max.y, min.z}) <= 0 ? 1 : 0;
		r += planes[index].calculate_distance({max.x, max.y, min.z}) <= 0 ? 1 : 0;
		r += planes[index].calculate_distance({min.x, min.y, max.z}) <= 0 ? 1 : 0;
		r += planes[index].calculate_distance({max.x, min.y, max.z}) <= 0 ? 1 : 0;
		r += planes[index].calculate_distance({min.x, max.y, max.z}) <= 0 ? 1 : 0;
		r += planes[index].calculate_distance({max.x, max.y, max.z}) <= 0 ? 1 : 0;
		if (r == 8)
			return false;
	}

	//	頂点未算出なら平面判定のみ
	if (!has_corners)
		return true;

	r = 0;
	for (int index = 0; index < 8; ++index)
		r += ((corners[index].x <= min.x) ? 1 : 0);
	if (r == 8)
		return false;
	r = 0;
	for (int index = 0; index < 8; ++index)
		r += ((corners[index].x >= max.x) ? 1 : 0);
	if (r == 8)
		return false;

	r = 0;
	for (int index = 0; index < 8; ++index)
		r += ((corners[index].y <= min.y) ? 1 : 0);
	if (r == 8)
		return false;
	r = 0;
	for (int index = 0; index < 8; ++index)
		r += ((corners[index].y >= max.y) ? 1 : 0);
	if (r == 8)
		return false;

	r = 0;
	for (int index = 0; index < 8; ++index)
		r += ((corners[index].z <= min.z) ? 1 : 0);
	if (r == 8)
		return false;
	r = 0;
	for (int index = 0; index < 8; ++index)
		r += ((corners[index].z >= max.z) ? 1 : 0);
	if (r == 8)
		return false;

	return true;
}

void frustum_culling::create_planes(const math::float4x4& view_projection_matrix)
{
	const math::vector4f col0(view_projection_matrix.m[0][0], view_projection_matrix.m[1][0], view_projection_matrix.m[2][0], view_projection_matrix.m[3][0] );
	const math::vector4f col1(view_projection_matrix.m[0][1], view_projection_matrix.m[1][1], view_projection_matrix.m[2][1], view_projection_matrix.m[3][1] );
	const math::vector4f col2(view_projection_matrix.m[0][2], view_projection_matrix.m[1][2], view_projection_matrix.m[2][2], view_projection_matrix.m[3][2] );
	const math::vector4f col3(view_projection_matrix.m[0][3], view_projection_matrix.m[1][3], view_projection_matrix.m[2][3], view_projection_matrix.m[3][3] );

	planes[surface_left].set_parameter((col3 + col0).to_float4());
	planes[surface_right].set_parameter((col3 - col0).to_float4());

	planes[surface_bottom].set_parameter((col3 + col1).to_float4());
	planes[surface_top].set_parameter((col3 - col1).to_float4());

	planes[surface_near].set_parameter(col2.to_float4());
	planes[surface_far].set_parameter((col3 - col2).to_float4());

	for (size_t index = 0; index < surface_max; ++index)
	{
		planes[index].normlize();
	}
}

void frustum_culling::create_frustum_corners(const math::float4x4& inverse_view_projection_matrix)
{
	create_frustum_corners(math::matrixf(inverse_view_projection_matrix));
}

void frustum_culling::create_frustum_corners(const math::matrixf& inverse_view_projection_matrix)
{
	const math::vector4f base_corners[] =
	{
		math::vector4f(-1, -1, 0, 1),
		math::vector4f( 1, -1, 0, 1),
		math::vector4f( 1,  1, 0, 1),
		math::vector4f(-1,  1, 0, 1),
		math::vector4f(-1, -1,  1, 1),
		math::vector4f( 1, -1,  1, 1),
		math::vector4f( 1,  1,  1, 1),
		math::vector4f(-1,  1,  1, 1)
	};
	
	for (int index = 0; index < 8; ++index)
	{
		const math::vector4f point = math::vector4f::transform(base_corners[index], inverse_view_projection_matrix);
		float w = point.w;
		//	無限遠の頂点は使用しない
		if (fabsf(w) <= 1e-6f)
		{
			has_corners = false;
			return;
		}
		corners[index] = (point * (1.0f / w)).to_float4();
	}
	has_corners = true;
}

