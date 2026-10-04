#pragma once

#include	"collision.h"

namespace collision
{
	class bounding_sphere final : public bounding_object
	{
	public:
		types get_type() const override {return types::sphere;}

		double get_radius() const;
		void set_radius(double radius);

	public:
		bounding_sphere();
		bounding_sphere(const math::double3& position, double radius);
		~bounding_sphere() override;

		bool intersect_point(const math::double3& point) const override;
		bool intersect_segment(const math::double3& start, const math::double3& end, intersect_data* intersect = nullptr) const override;
		bool intersect_segment(math::vector3d Start, math::vector3d End, intersect_data* intersect = nullptr) const;
		bool intersect_sphere(const math::double3& point, double radius, intersect_data* intersect = nullptr) const;
		bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const override;
		bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const override;
		bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const override;

		//	@remainder	ñ¢åüèÿ
		bool intersect_sphere_cast(const math::double3& start, const math::double3& end, double radius, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule_cast(const math::double3& start, const math::double3& end, double radius, double length, const math::double4& rotation, intersect_data* intersect = nullptr) const override;

		void include_corner(math::double3& min, math::double3& max) const override;
	};
}	//	namespace collision
