#pragma once

#include	"collision.h"

namespace collision
{
	class bounding_box : public bounding_object
	{
	public:
		types get_type() const override {return types::box;}

		math::matrixf get_transeform_matrix() const;
		math::float4x4 get_transeform_float4x4() const;

	public:
		bounding_box();
		bounding_box(const math::float3& scale, const math::float4& rotation, const math::float3& position);
		bounding_box(const math::float3& scale, const math::float3& y_axis, float angle, const math::float3& position);
		~bounding_box() override;

		float calculate_point_to_length(const math::float3& point) const;

		bool intersect_point(const math::float3& point) const override;
		bool intersect_segment(const math::float3& start, const math::float3& end, intersect_data* intersect = nullptr) const override;
		bool intersect_sphere(const math::float3& point, float radius, intersect_data* intersect = nullptr) const;
		bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const override;
		bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const override;
		bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const override;

		bool intersect_sphere_cast(const math::float3& start, const math::float3& end, float radius, intersect_data* intersect = nullptr) const override;
		bool intersect_sphere_cast(math::vector3f Start, math::vector3f End, float radius, intersect_data* intersect = nullptr) const;
		bool intersect_capsule_cast(const math::float3& start, const math::float3& end, float radius, float length, const math::float4& rotation, intersect_data* intersect = nullptr) const override;

		void include_corner(math::float3& min, math::float3& max) const override;

	public:
		bounding_box multiply_obb(const math::matrixf& matrix) const;
		bounding_box multiply_obb(const math::float4x4& matrix) const;

		void calculate_vertex(math::float3* vertices, size_t vertex_count) const;
	};

	namespace inner
	{
		//	AABB
		//	直接使用することは基本的に禁止
		class axis_aligned_bounding_box : public bounding_object
		{
		public:
			types get_type() const override {return types::box;}

			math::matrixf get_transeform_matrix() const;
			math::float4x4 get_transeform_float4x4() const;

		public:
			axis_aligned_bounding_box();
			axis_aligned_bounding_box(const math::float3& scale, const math::float3& position);
			~axis_aligned_bounding_box() override;

			float calculate_point_to_length(const math::float3& point) const;
			bool intersect_point(const math::float3& point) const override;
			bool intersect_segment(const math::float3& start, const math::float3& end, intersect_data* intersect = nullptr) const override;
			bool intersect_sphere(const math::float3& point, float radius, intersect_data* intersect = nullptr) const;
			bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const override;
			bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const override;
			bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const override;
			bool intersect_box(const axis_aligned_bounding_box& box) const;
			bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const override;

			//	@remainder	未検証
			bool intersect_sphere_cast(const math::float3& start, const math::float3& end, float radius, intersect_data* intersect = nullptr) const override;
			bool intersect_sphere_cast(math::vector3f Start, math::vector3f End, float radius, intersect_data* intersect = nullptr) const;
			bool intersect_capsule_cast(const math::float3& start, const math::float3& end, float radius, float length, const math::float4& rotation, intersect_data* intersect = nullptr) const override;

			void include_corner(math::float3& min, math::float3& max) const;
			void include_corner(math::vector3f& Min, math::vector3f& Max) const;

			void calculate_vertex(math::float3* vertices, size_t vertex_count) const;
		};
	}	//	namespace inner
}	//	namespace collision
