#pragma once

#include	"collision.h"
#include	"line.h"
#include	"box.h"

namespace collision
{

	class bounding_mesh : public bounding_object
	{
	public:
		static constexpr std::string_view extension = "ebmo";

		struct triangle
		{
			std::uint32_t a, b, c;	//	座標番号

			//	事前計算
			math::float3 normal;		//	法線
			math::float3 ab, bc, ca;	//	辺
			math::float3 min, max;		//	最小最大
			collision::linef lab, lbc, lca;	//	線分

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		struct block_data
		{
			math::float3 center;
			std::vector<size_t> triangles;

			inner::axis_aligned_bounding_box bounds;
			template<class Archive>
			void serialize(Archive& archive, int version);
		};

	public:
		types get_type() const override {return types::mesh;}

		const math::double3& get_local_scale() const override;
		void set_local_scale(const math::double3& scale) override;

		const std::vector<math::float3>& get_positions() const {return positions;}
		const std::vector<triangle>& get_triangles() const {return triangles;}
		const std::vector<block_data>& get_block_datas() const {return block_datas;}
		const math::float3& get_area_unit() const {return area_unit;}
		const math::uint3& get_separate_count() const {return separate_count;}

	public:
		bounding_mesh();
		bounding_mesh(std::vector<math::float3>& positions, std::vector<triangle>& triangles, std::uint32_t separate_level = 4);
		~bounding_mesh() override;

		bool serialize(std::string_view file_path);
		bool deserialize(std::string_view file_path);

		bool intersect_point(const math::double3& point) const override;
		bool intersect_segment(const math::double3& start, const math::double3& end, intersect_data* intersect = nullptr) const override;
		bool intersect_sphere(const math::double3& point, double radius, intersect_data* intersect = nullptr) const;
		bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const override;
		bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const override;
		bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const override;

		bool intersect_sphere_cast(const math::double3& start, const math::double3& end, double radius, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule_cast(const math::double3& start, const math::double3& end, double radius, double length, const math::double4& rotation, intersect_data* intersect = nullptr) const override;

		void include_corner(math::double3& min, math::double3& max) const override;

	public:
		//	点が閉じたメッシュの内側にあるかを判定
		bool is_point_inside(const math::double3& point) const;
		//	点からメッシュ表面までの最短距離を求める
		bool distance_to_point(const math::double3& point, double& out_distance, math::double3* out_nearest_point = nullptr, math::double3* out_normal = nullptr) const;

	private:
		bool intersect_point_inner(const math::float3& point, float& nearlest, const std::vector<size_t>& triangle_indices) const;
		bool intersect_segment_inner(const math::float3& start, const math::float3& end, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_segment_inner_moller(const math::float3& start, const math::float3& end, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_sphere_inner(const math::float3& point, float radius, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_capsule_inner(const bounding_capsule& local_capsule, math::vector3f CapsuleMin, math::vector3f CapsuleMax, const math::matrixd& TransformMatrix, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_box_inner(math::vector3f Center, math::vector3f AxisX, math::vector3f AxisY, math::vector3f AxisZ, const math::float3& half_extent, math::vector3f BoxMin, math::vector3f BoxMax, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_sphere_cast_inner(const collision::linef& sweep_line, float radius, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;

	private:
		float calculate_segment_distance(const collision::linef& line, const triangle& triangle,
										 math::float3& nearlest_point_segment, math::float3& nearlest_point_triangle, bool& nearlest_point_edghFlg) const;

	private:
		std::vector<math::float3> positions;
		std::vector<triangle> triangles;

		math::float3 area_min;
		math::float3 area_max;
		math::float3 area_size;
		math::float3 area_unit;
		math::uint3 separate_count;
		std::vector<block_data> block_datas;
	};

}	//	namespace collision
