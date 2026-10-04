#pragma once

#include	"collision.h"

namespace collision
{
	class bounding_sphere final : public bounding_object
	{
	public:
		types get_type() const override {return types::sphere;}

		float get_radius() const;
		void set_radius(float radius);

	public:
		bounding_sphere();
		bounding_sphere(const math::float3& position, float radius);
		~bounding_sphere() override;

		bool intersect_point(const math::float3& point) const override;
		bool intersect_segment(const math::float3& start, const math::float3& end, intersect_data* intersect = nullptr) const override;
		bool intersect_segment(math::vector3f Start, math::vector3f End, intersect_data* intersect = nullptr) const;
		bool intersect_sphere(const math::float3& point, float radius, intersect_data* intersect = nullptr) const;
		bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const override;
		bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const override;
		bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const override;

		//	@remainder	–¢ŒŸØ
		bool intersect_sphere_cast(const math::float3& start, const math::float3& end, float radius, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule_cast(const math::float3& start, const math::float3& end, float radius, float length, const math::float4& rotation, intersect_data* intersect = nullptr) const override;

		void include_corner(math::float3& min, math::float3& max) const override;
	};
}	//	namespace collision
