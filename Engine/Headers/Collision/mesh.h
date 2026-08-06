#pragma once

#include	"collision.h"
#include	"model_resource.h"

namespace collision
{
	using UINT = unsigned int;

	class bounding_mesh : public bounding_object
	{
	public:
		struct triangle
		{
			UINT a, b, c;	//	座標番号

			//	計算対象
			DirectX::XMFLOAT3 normal;		//	法線
			DirectX::XMFLOAT3 ab, bc, ca;	//	辺
			DirectX::XMFLOAT3 min, max;		//	最小最大
			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		struct block_data
		{
			DirectX::XMFLOAT3 center;
			std::vector<size_t> triangles;

			inner::axis_aligned_bounding_box bounds;
			template<class Archive>
			void serialize(Archive& archive, int version);
		};

	public:
		types get_type() const override {return types::mesh;}

		const DirectX::XMFLOAT3& get_local_scale() const override;
		void set_local_scale(const DirectX::XMFLOAT3& scale) override;

		const std::vector<DirectX::XMFLOAT3>& get_positions() const {return positions;}
		const std::vector<triangle>& get_triangles() const {return triangles;}
		const std::vector<block_data>& get_block_datas() const {return block_datas;}
		const DirectX::XMFLOAT3& get_area_unit() const {return area_unit;}
		const DirectX::XMUINT3& get_separate_count() const {return separate_count;}

	public:
		bounding_mesh();
		bounding_mesh(std::vector<DirectX::XMFLOAT3>& positions, std::vector<triangle>& triangles, UINT separate_level = 4);
		~bounding_mesh() override;

		bool serialize(std::string_view file_path);
		bool deserialize(std::string_view file_path);

		bool intersect_point(const DirectX::XMFLOAT3& point) const override;
		bool intersect_segment(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, intersect_data* intersect = nullptr) const override;
		bool intersect_sphere(const DirectX::XMFLOAT3& point, float radius, intersect_data* intersect = nullptr) const;
		bool intersect_sphere(const bounding_sphere& sphere, intersect_data* intersect = nullptr) const override;
		bool intersect_capsule(const bounding_capsule& capsule, intersect_data* intersect = nullptr) const override;
		bool intersect_box(const bounding_box& box, intersect_data* intersect = nullptr) const override;
		bool intersect_mesh(const bounding_mesh& mesh, intersect_data* intersect = nullptr) const override;

		bool intersect_sphere_cast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float radius, intersect_data* intersect = nullptr) const override;

		void include_corner(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max) const override;

	private:
		bool intersect_point_inner(const DirectX::XMFLOAT3& point, float& nearlest, const std::vector<size_t>& triangle_indices) const;
		bool intersect_segment_inner(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_segment_inner_moller(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_sphere_inner(const DirectX::XMFLOAT3& point, float radius, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_capsule_inner(const bounding_capsule& local_capsule, DirectX::XMVECTOR CapsuleMin, DirectX::XMVECTOR CapsuleMax, DirectX::XMMATRIX TransformMatrix, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_box_inner(DirectX::XMVECTOR Center, DirectX::XMVECTOR AxisX, DirectX::XMVECTOR AxisY, DirectX::XMVECTOR AxisZ, const DirectX::XMFLOAT3& half_extent, DirectX::XMVECTOR BoxMin, DirectX::XMVECTOR BoxMax, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;
		bool intersect_sphere_cast_inner(const collision::line& sweep_line, float radius, const std::vector<size_t>& triangle_indices, intersect_data* intersect = nullptr) const;

	private:
		float calculate_segment_distance(const collision::line& line, const triangle& triangle,
										 DirectX::XMFLOAT3& nearlest_point_segment, DirectX::XMFLOAT3& nearlest_point_triangle, bool& nearlest_point_edghFlg) const;

	private:
		std::vector<DirectX::XMFLOAT3> positions;
		std::vector<triangle> triangles;

		DirectX::XMFLOAT3 area_min;
		DirectX::XMFLOAT3 area_max;
		DirectX::XMFLOAT3 area_size;
		DirectX::XMFLOAT3 area_unit;
		DirectX::XMUINT3 separate_count;
		std::vector<block_data> block_datas;
	};

	struct generate_bounding_mesh_data
	{
		DirectX::XMMATRIX WorldMatrix;	// ワールド行列
		bool reverse_flag = true;		// ポリゴン反転処理フラグ
		UINT separate_level = 4;		// 空間分割数
	};
	bool generate_bounding_mesh(bounding_mesh& out_mesh, model_resource_ptr resource, size_t index, generate_bounding_mesh_data data);
}	//	namespace collision
