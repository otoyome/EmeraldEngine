// RAYCAST
#include "Collision/raycast.h"


int intersect_ray_triangles
(
	const float* positions,
	const uint32_t offset,
	const uint32_t stride,
	const uint32_t* indices,
	const size_t index_count,
	const math::float4& ray_position,
	const math::float4& ray_direction,
	math::float4& intersection_point,
	math::float3& intersection_normal,
	float& distance,
	const bool RHS
)
{
	const bool CCW{ RHS };
	const int C0{ 0 };
	const int C1{ CCW ? 1 : 2 };
	const int C2{ CCW ? 2 : 1 };

	int intersection_count{ 0 };
	int intersected_triangle_index{ -1 };

	const float ray_length_limit{ distance };
	float closest_distance{ FLT_MAX };

	math::vector3f XP{}, XN{};
	const math::vector3f P(ray_position.x, ray_position.y, ray_position.z);
	const math::vector3f D{ (math::vector3f(ray_direction.x, ray_direction.y, ray_direction.z)).normalized() };

	using byte = uint8_t;
	const byte* p{ reinterpret_cast<const byte*>(positions) + offset };
	const size_t triangle_count{ index_count / 3 };
	for (size_t triangle_index = 0; triangle_index < triangle_count; triangle_index++)
	{
		const math::vector3f A(*reinterpret_cast<const math::float3*>(p + indices[triangle_index * 3 + C0] * stride));
		const math::vector3f B(*reinterpret_cast<const math::float3*>(p + indices[triangle_index * 3 + C1] * stride));
		const math::vector3f C(*reinterpret_cast<const math::float3*>(p + indices[triangle_index * 3 + C2] * stride));

		math::vector3f Q{};

		const math::vector3f N{ (math::vector3f::cross(B - A, C - A)).normalized() };
		const float d{ math::vector3f::dot(N, A) };

		const float denominator{ math::vector3f::dot(N, D) };
		if (denominator < 0)
		{
			const float numerator{ d - math::vector3f::dot(N, P) };
			const float t{ numerator / denominator };

			if (t > 0 && t < ray_length_limit)
			{
				Q = P + (D * t);

				const math::vector3f QA{ A - Q };
				const math::vector3f QB{ B - Q };
				const math::vector3f QC{ C - Q };

				math::vector3f U{ math::vector3f::cross(QB, QC) };
				math::vector3f V{ math::vector3f::cross(QC, QA) };
				if (math::vector3f::dot(U, V) < 0)
				{
					continue;
				}

				math::vector3f W{ math::vector3f::cross(QA, QB) };
				if (math::vector3f::dot(U, W) < 0)
				{
					continue;
				}
				if (math::vector3f::dot(V, W) < 0)
				{
					continue;
				}

				if (t < closest_distance)
				{
					closest_distance = t;
					intersected_triangle_index = static_cast<int>(triangle_index);
					XP = Q;
					XN = N;
				}
				intersection_count++;
			}
		}
	}
	if (intersection_count > 0)
	{
		intersection_point = math::float4(XP.x, XP.y, XP.z, 1.0f);
		intersection_normal = XN.to_float3();
		distance = closest_distance;
	}
	return intersected_triangle_index;
}
