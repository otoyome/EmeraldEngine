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
			math::float3 point = {};	// 接触時の中心
			math::float3 point2 = {};	// 相手側接触点
			math::float3 normal = {};	// 法線
			float length = 0.0f;			// 距離
		};

	public:
		intersect_data();
		virtual ~intersect_data();

		void add(const infomation& infomation);
		void add(const math::float3& point, const math::float3& normal, float length);
		void clear();

		bool multiply_matrix(std::size_t index, math::float4x4 matrix);
		bool multiply_matrix(std::size_t index, math::matrixf matrix);
		void multiply_matrices(std::size_t begin, math::matrixf matrix);
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

		virtual const math::float3& get_local_scale() const;
		virtual void set_local_scale(const math::float3& scale);

		const math::float4& get_rotation() const;
		void set_rotation(const math::float4& rotation);

		const math::float3& get_position() const;
		void set_position(const math::float3& position);

		math::float4x4 get_transform_float4x4() const;
		math::matrixf get_transform_matrix() const;

		math::float4x4 get_inverse_transform_float4x4() const;
		math::matrixf get_inverse_transform_matrix() const;

	public:
		bounding_object();
		bounding_object(const math::float3& local_scale, const math::float4& rotation, const math::float3& position);
		virtual	~bounding_object();

		bool intersection(const bounding_object* object, intersect_data* intersect = nullptr) const;
		virtual bool intersect_point(const math::float3& point) const = 0;
		virtual bool intersect_segment(const math::float3& start, const math::float3& end, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const = 0;

		virtual bool intersect_sphere_cast(const math::float3& start, const math::float3& end, float radius, intersect_data* intersect = nullptr) const = 0;

		virtual bool intersect_capsule_cast(const math::float3& start, const math::float3& end, float radius, float length, const math::float4& rotation, intersect_data* intersect = nullptr) const = 0;

		//	スフィアキャストを流用して軸毎にスフィアキャストを行う方式
		bool intersect_capsule_cast_by_axis_spheres(const math::float3& start, const math::float3& end,
			float radius, float length, math::vector3f axis, intersect_data* intersect = nullptr) const;

		virtual void include_corner(math::float3& min, math::float3& max) const = 0;

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
		math::float3 local_scale;
		math::float4 rotation;
		math::float3 position;
		math::float4x4 transform_matrix;
		math::float4x4 inverse_transform_matrix;
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

	bool intersect_ray_vs_sphere(math::vector3f start, math::vector3f end,
											 math::vector3f sphere_pos, float radius,
											 math::vector3f& hit_pos, float& hit_distance);

	bool intersect_ray_vs_cylinder(math::vector3f start, math::vector3f end,
											   math::vector3f start_cylinder, math::vector3f end_cylinder,
											   float radius, math::vector3f& hit_pos, float& hit_distance);

	//	キャスト結果の追加
	void add_cast_hit(intersect_data* intersect, math::vector3f center, math::vector3f contact, math::vector3f normal, float length);

	//	キャスト開始時の重なり状態
	enum class cast_start_overlap
	{
		none,		//	重なり無し
		hit,		//	接近中
		leaving,	//	離脱中
	};

	//	キャスト開始時の重なり判定
	cast_start_overlap check_cast_start_overlap(math::vector3f start, math::vector3f end, math::vector3f nearest_point,
															float radius, math::vector3f fallback_normal, intersect_data* intersect);
}	//	namespace collision
