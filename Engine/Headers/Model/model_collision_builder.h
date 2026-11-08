#pragma once

#include <filesystem>

#include "Collision/mesh.h"
#include "Collision/cubic_sdf.h"
#include "Collision/combined_mesh.h"

#include "Model/model_resource.h"

namespace collision
{
	struct generate_bounding_mesh_data
	{
		math::matrixf WorldMatrix;	// ワールド行列
		bool reverse_flag = true;	// ポリゴン反転処理フラグ
		int lod_level = -1;			// LODメッシュから生成 -1: ベースメッシュ / 0以上: lod_meshs[lod_level] のインデックスから生成
	};
	bool generate_bounding_mesh(bounding_mesh& out_mesh, model_resource_ptr resource, size_t index, generate_bounding_mesh_data data);

	//	複数メッシュを1つの当たり判定メッシュへ統合して生成
	bool generate_combined_bounding_mesh(bounding_mesh& out_mesh, model_resource_ptr resource, const std::vector<size_t>& indices, generate_bounding_mesh_data data);

	//	当たり判定メッシュにした時の頂点範囲を求める
	bool calculate_bounding_mesh_bounds(model_resource_ptr resource, size_t index, const generate_bounding_mesh_data& data, math::float3& out_min, math::float3& out_max);

	//	 model_resourceから cubic Signed Distance Field を生成
	bool create_cubic_sdf(cubic_sdf& out_sdf, model_resource_ptr resource, size_t index, const generate_bounding_mesh_data& mesh_data, uint32_t resolution = 32, float bounds_padding_rate = 0.1f);
}	//	namespace collision
