#pragma once

#include	"collision.h"
#include	"line.h"

namespace collision
{
	class bounding_capsule final : public bounding_object
	{
	public:
		types get_type() const override {return types::capsule;}

		const lined& get_line() const {return line;}

		double get_radius() const;
		void set_radius(double radius);
		double get_length() const;
		void set_length(double length);

		math::vector3d get_axis_vector() const;
		math::double3 get_axis_float3() const;

	public:
		bounding_capsule();
		bounding_capsule(const math::double3& position, const math::double4& rotation, double radius, double length);
		~bounding_capsule() override;

		bool intersect_point(const math::double3& point) const override;
		bool intersect_segment(const math::double3& start, const math::double3& end, intersect_data* intersect = nullptr) const override;
		bool intersect_sphere(const math::double3& point, double radius, intersect_data* intersect = nullptr) const;
		bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const override;
		bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const override;
		bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const override;

		//	@remainder	ñ¢åüèÿ
		bool intersect_sphere_cast(const math::double3& start, const math::double3& end, double radius, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule_cast(const math::double3& start, const math::double3& end, double radius, double length, const math::double4& rotation, intersect_data* intersect = nullptr) const override;

		void include_corner(math::double3& min, math::double3& max) const override;

	protected:
		void on_change_parameter() override;

	private:
		lined line;
	};
}	//	namespace collision