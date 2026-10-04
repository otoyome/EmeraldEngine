#pragma once

#include <vector>
#include <memory>
#include "Math/math_all.h"

namespace collision
{
	class intersect_data
	{
	public:
		struct infomation
		{
			math::double3 point = {};	// 接触時の中心
			math::double3 point2 = {};	// 相手側接触点
			math::double3 normal = {};	// 法線
			double length = 0.0f;			// 距離
		};

	public:
		void add(const infomation& infomation);
		void add(const math::double3& point, const math::double3& normal, double length);
		void add(const math::float3& point, const math::float3& normal, float length);
		void clear();
		void swap(intersect_data& other) noexcept { infomations.swap(other.infomations); }

		bool multiply_matrix(std::size_t index, const math::double4x4& matrix);
		bool multiply_matrix(std::size_t index, const math::matrixd& matrix);
		void multiply_matrices(std::size_t begin, const math::matrixd& matrix);
		void flip_normals_from(std::size_t begin);
		void append(const intersect_data& data);

		std::size_t size() const;

	public:
		const infomation& operator[](std::size_t index) const;
		infomation& operator[](std::size_t index);

	private:
		std::vector<infomation>	infomations;
	};

	class bounding_sphere;
	class bounding_box;
	class bounding_capsule;
	class bounding_mesh;

	class bounding_object
	{
	public:
		enum class types
		{
			sphere	= 0,
			capsule,
			box,

			mesh,
		};

	public:
		virtual types get_type() const = 0;

		virtual const math::double3& get_local_scale() const;
		virtual void set_local_scale(const math::double3& scale);

		const math::double4& get_rotation() const;
		void set_rotation(const math::double4& rotation);

		const math::double3& get_position() const;
		void set_position(const math::double3& position);

		math::float4x4 get_transform_float4x4() const;
		math::matrixd get_transform_matrix() const;

		math::float4x4 get_inverse_transform_float4x4() const;
		math::matrixd get_inverse_transform_matrix() const;

	public:
		bounding_object();
		bounding_object(const math::double3& local_scale, const math::double4& rotation, const math::double3& position);
		virtual	~bounding_object();

		bool intersection(const bounding_object* object, intersect_data* intersect = nullptr) const;
		virtual bool intersect_point(const math::double3& point) const = 0;
		virtual bool intersect_segment(const math::double3& start, const math::double3& end, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const = 0;

		virtual bool intersect_sphere_cast(const math::double3& start, const math::double3& end, double radius, intersect_data* intersect = nullptr) const = 0;

		virtual bool intersect_capsule_cast(const math::double3& start, const math::double3& end, double radius, double length, const math::double4& rotation, intersect_data* intersect = nullptr) const = 0;

		//	スフィアキャストを流用して軸毎にスフィアキャストを行う方式
		bool intersect_capsule_cast_by_axis_spheres(const math::double3& start, const math::double3& end,
			double radius, double length, math::vector3d axis, intersect_data* intersect = nullptr) const;

		virtual void include_corner(math::double3& min, math::double3& max) const = 0;

	protected:
		virtual	void on_change_parameter();

		//	相手側関数へ委譲し法線を自分基準へ反転
		template<typename function>
		static bool delegate_flipped(intersect_data* intersect, function&& func)
		{
			std::size_t begin = intersect ? intersect->size() : 0;
			if (!func())
				return false;
			if (intersect)
				intersect->flip_normals_from(begin);
			return true;
		}

	private:
		math::double3 local_scale;
		math::double4 rotation;
		math::double3 position;
		math::double4x4 transform_matrix;
		math::double4x4 inverse_transform_matrix;
	};

	//	レイキャスト
	struct triangles
	{
		const math::float3* positions = nullptr;
		const unsigned int* indices = nullptr;
		const math::float3* polygon_normal = nullptr;
		size_t index_start = 0;
		size_t index_count = 0;
		bool reverse_index = false;
		bool indices_stride_16 = false;
	};
	bool ray_cast(const math::float3& start, const math::float3& end, const triangles& triangles, intersect_data* result);
	bool ray_cast(math::vector3f start, math::vector3f end, const triangles& triangles, intersect_data* result);
	bool ray_cast_16(math::vector3f start, math::vector3f end, const triangles& triangles, intersect_data* result);
	bool ray_cast_32(math::vector3f start, math::vector3f end, const triangles& triangles, intersect_data* result);

	bool intersect_ray_vs_sphere(math::vector3d start, math::vector3d end,
											 math::vector3d sphere_pos, double radius,
											 math::vector3d& hit_pos, double& hit_distance);

	bool intersect_ray_vs_cylinder(math::vector3d start, math::vector3d end,
											   math::vector3d start_cylinder, math::vector3d end_cylinder,
											   double radius, math::vector3d& hit_pos, double& hit_distance);

	//	キャスト結果の追加
	void add_cast_hit(intersect_data* intersect, math::vector3d center, math::vector3d contact, math::vector3d normal, double length);

	//	キャスト開始時の重なり状態
	enum class cast_start_overlap
	{
		none,		//	重なり無し
		hit,		//	接近中
		leaving,	//	離脱中
	};

	//	キャスト開始時の重なり判定
	cast_start_overlap check_cast_start_overlap(math::vector3d start, math::vector3d end, math::vector3d nearest_point,
															double radius, math::vector3d fallback_normal, intersect_data* intersect);
}	//	namespace collision
