#pragma once

#include <DirectXMath.h>
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
			DirectX::XMFLOAT3 point = {};	// 接触時の中心
			DirectX::XMFLOAT3 point2 = {};	// 相手側接触点
			DirectX::XMFLOAT3 normal = {};	// 法線
			float length = 0.0f;			// 距離
		};

	public:
		intersect_data();
		virtual ~intersect_data();

		void add(const infomation& infomation);
		void add(const DirectX::XMFLOAT3& point, const DirectX::XMFLOAT3& normal, float length);
		void clear();

		bool multiply_matrix(std::size_t index, DirectX::XMFLOAT4X4 matrix);
		bool multiply_matrix(std::size_t index, DirectX::XMMATRIX matrix);
		void multiply_matrices(std::size_t begin, DirectX::XMMATRIX matrix);
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

		virtual const DirectX::XMFLOAT3& get_local_scale() const;
		virtual void set_local_scale(const DirectX::XMFLOAT3& scale);

		const DirectX::XMFLOAT4& get_rotation() const;
		void set_rotation(const DirectX::XMFLOAT4& rotation);

		const DirectX::XMFLOAT3& get_position() const;
		void set_position(const DirectX::XMFLOAT3& position);

		DirectX::XMFLOAT4X4 get_transform_float4x4() const;
		DirectX::XMMATRIX get_transform_matrix() const;

		DirectX::XMFLOAT4X4 get_inverse_transform_float4x4() const;
		DirectX::XMMATRIX get_inverse_transform_matrix() const;

	public:
		bounding_object();
		bounding_object(const DirectX::XMFLOAT3& local_scale, const DirectX::XMFLOAT4& rotation, const DirectX::XMFLOAT3& position);
		virtual	~bounding_object();

		bool intersection(const bounding_object* object, intersect_data* intersect = nullptr) const;
		virtual bool intersect_point(const DirectX::XMFLOAT3& point) const = 0;
		virtual bool intersect_segment(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const = 0;
		virtual bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const = 0;

		virtual bool intersect_sphere_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float radius, intersect_data* intersect = nullptr) const = 0;

		virtual bool intersect_capsule_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float radius, float length, const DirectX::XMFLOAT4& rotation, intersect_data* intersect = nullptr) const = 0;

		//	スフィアキャストを流用して軸毎にスフィアキャストを行う方式
		bool intersect_capsule_cast_by_axis_spheres(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
			float radius, float length, DirectX::XMVECTOR axis, intersect_data* intersect = nullptr) const;

		virtual void include_corner(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max) const = 0;

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
		DirectX::XMFLOAT3 local_scale;
		DirectX::XMFLOAT4 rotation;
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4X4 transform_matrix;
		DirectX::XMFLOAT4X4 inverse_transform_matrix;
	};

	//	レイキャスト
	struct triangles
	{
		const DirectX::XMFLOAT3* positions = nullptr;
		const unsigned int* indices = nullptr;
		const DirectX::XMFLOAT3* polygon_normal = nullptr;
		size_t index_start = 0;
		size_t index_count = 0;
		bool reverse_index = false;
		bool indices_stride_16 = false;
	};
	bool ray_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const triangles& triangles, intersect_data* result);
	bool ray_cast(DirectX::XMVECTOR start, DirectX::XMVECTOR end, const triangles& triangles, intersect_data* result);
	bool ray_cast_16(DirectX::XMVECTOR start, DirectX::XMVECTOR end, const triangles& triangles, intersect_data* result);
	bool ray_cast_32(DirectX::XMVECTOR start, DirectX::XMVECTOR end, const triangles& triangles, intersect_data* result);

	bool XM_CALLCONV intersect_ray_vs_sphere(DirectX::XMVECTOR start, DirectX::XMVECTOR end,
											 DirectX::XMVECTOR sphere_pos, float radius,
											 DirectX::XMVECTOR& hit_pos, float& hit_distance);

	bool XM_CALLCONV intersect_ray_vs_cylinder(DirectX::XMVECTOR start, DirectX::XMVECTOR end,
											   DirectX::XMVECTOR start_cylinder, DirectX::XMVECTOR end_cylinder,
											   float radius, DirectX::XMVECTOR& hit_pos, float& hit_distance);

	//	キャスト結果の追加
	void XM_CALLCONV add_cast_hit(intersect_data* intersect, DirectX::XMVECTOR center, DirectX::XMVECTOR contact, DirectX::XMVECTOR normal, float length);

	//	キャスト開始時の重なり状態
	enum class cast_start_overlap
	{
		none,		//	重なり無し
		hit,		//	接近中
		leaving,	//	離脱中
	};

	//	キャスト開始時の重なり判定
	cast_start_overlap XM_CALLCONV check_cast_start_overlap(DirectX::XMVECTOR start, DirectX::XMVECTOR end, DirectX::XMVECTOR nearest_point,
															float radius, DirectX::XMVECTOR fallback_normal, intersect_data* intersect);
}	//	namespace collision
