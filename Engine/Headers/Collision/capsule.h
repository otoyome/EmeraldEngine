#pragma once

#include	"collision.h"
#include	"line.h"

namespace collision
{
	class bounding_capsule final : public bounding_object
	{
	public:
		types get_type() const override {return types::capsule;}

		const line& get_line() const {return line;}

		float get_radius() const;
		void set_radius(float radius);
		float get_length() const;
		void set_length(float length);

		math::vector3f get_axis_vector() const;
		math::float3 get_axis_float3() const;

	public:
		bounding_capsule();
		bounding_capsule(const math::float3& position, const math::float4& rotation, float radius, float length);
		~bounding_capsule() override;

		bool intersect_point(const math::float3& point) const override;
		bool intersect_segment(const math::float3& start, const math::float3& end, intersect_data* intersect = nullptr) const override;
		bool intersect_sphere(const math::float3& point, float radius, intersect_data* intersect = nullptr) const;
		bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const override;
		bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const override;
		bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const override;

		//	@remainder	ñ¢åüèÿ
		bool intersect_sphere_cast(const math::float3& start, const math::float3& end, float radius, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule_cast(const math::float3& start, const math::float3& end, float radius, float length, const math::float4& rotation, intersect_data* intersect = nullptr) const override;

		void include_corner(math::float3& min, math::float3& max) const override;

	protected:
		void on_change_parameter() override;

	private:
		line line;
	};
}	//	namespace collision