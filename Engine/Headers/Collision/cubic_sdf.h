#pragma once

#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <string_view>
#include "System/math_utility.h"
#include "model_resource.h"

namespace collision
{
	class bounding_mesh;
	struct generate_bounding_mesh_data;

	//	立体形状のSDF
	struct cubic_sdf
	{
		static constexpr std::string_view extension = "esdf";

		uint32_t resolution = 0;
		DirectX::XMFLOAT3 bounds_min = { 0, 0, 0 };
		DirectX::XMFLOAT3 bounds_max = { 0, 0, 0 };
		std::vector<float> voxels;

	public:
		bool is_valid() const;
		float voxel_size() const;
		size_t index_of(uint32_t x, uint32_t y, uint32_t z) const;
		DirectX::XMFLOAT3 voxel_center(uint32_t x, uint32_t y, uint32_t z) const;

		bool serialize(std::string_view file_path);
		bool deserialize(std::string_view file_path);
	};

	//	指定したメッシュから cubic Signed Distance Field を生成
	bool create_cubic_sdf(cubic_sdf& out_sdf, bounding_mesh& mesh, uint32_t resolution = 32, float bounds_padding_rate = 0.1f);

	//	 model_resourceから cubic Signed Distance Field を生成
	bool create_cubic_sdf(cubic_sdf& out_sdf, model_resource_ptr resource, size_t index, const generate_bounding_mesh_data& mesh_data, uint32_t resolution = 32, float bounds_padding_rate = 0.1f);

}	//	namespace collision
