#pragma once

#include "Collision/mesh.h"
#include "Collision/cubic_sdf.h"
#include "model_resource.h"

namespace collision
{
	struct generate_bounding_mesh_data
	{
		math::matrixf WorldMatrix;	// ワールド行列
		bool reverse_flag = true;	// ポリゴン反転処理フラグ
		UINT separate_level = 4;	// 空間分割数
		int lod_level = -1;			// LODメッシュから生成 -1: ベースメッシュ / 0以上: lod_meshs[lod_level] のインデックスから生成
	};
	bool generate_bounding_mesh(bounding_mesh& out_mesh, model_resource_ptr resource, size_t index, generate_bounding_mesh_data data);

	//	 model_resourceから cubic Signed Distance Field を生成
	bool create_cubic_sdf(cubic_sdf& out_sdf, model_resource_ptr resource, size_t index, const generate_bounding_mesh_data& mesh_data, uint32_t resolution = 32, float bounds_padding_rate = 0.1f);
}	//	namespace collision
