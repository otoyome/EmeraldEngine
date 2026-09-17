#pragma once

#include <DirectXMath.h>
#include <vector>
#include <memory>
#include "System/math_utility.h"

namespace collision
{
	class bounding_mesh;

	//	立体形状のSDF
	struct cubix_sdf
	{
		bool is_valid() const
		{
			return resolution > 0 && !voxels.empty();
		}

		//	ボクセルサイズ
		float voxel_size() const
		{
			return resolution > 0 ? (bounds_max.x - bounds_min.x) / static_cast<float>(resolution) : 0.0f;
		}

		//	ボクセルインデックス(x,y,z)から線形インデックスを求める
		size_t index_of(uint32_t x, uint32_t y, uint32_t z) const
		{
			return (static_cast<size_t>(z) * resolution + y) * resolution + x;
		}

		//	ボクセル中心のワールド(ローカル)座標を求める
		DirectX::XMFLOAT3 voxel_center(uint32_t x, uint32_t y, uint32_t z) const
		{
			const float size = voxel_size();
			return DirectX::XMFLOAT3
			{
				bounds_min.x + (static_cast<float>(x) + 0.5f) * size,
				bounds_min.y + (static_cast<float>(y) + 0.5f) * size,
				bounds_min.z + (static_cast<float>(z) + 0.5f) * size,
			};
		}

	public:
		uint32_t resolution = 0;
		DirectX::XMFLOAT3 bounds_min = { 0, 0, 0 };
		DirectX::XMFLOAT3 bounds_max = { 0, 0, 0 };
		std::vector<float> voxels;
	};

	//	指定したメッシュからcubic Signed Distance Fieldを生成する
	bool create_cubic_sdf(cubix_sdf& out_sdf, bounding_mesh& mesh, uint32_t resolution = 32, float bounds_padding_rate = 0.1f);

}	//	namespace collision
