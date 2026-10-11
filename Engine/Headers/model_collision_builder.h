#pragma once

#include <filesystem>

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

	//	@tool 複数メッシュを1つの当たり判定メッシュへ統合して生成
	bool generate_combined_bounding_mesh(bounding_mesh& out_mesh, model_resource_ptr resource, const std::vector<size_t>& indices, generate_bounding_mesh_data data);

	//	@tool 当たり判定メッシュにした時の頂点範囲（モデル空間）を求める
	bool calculate_bounding_mesh_bounds(model_resource_ptr resource, size_t index, const generate_bounding_mesh_data& data, math::float3& out_min, math::float3& out_max);

	//	@tool 統合した当たり判定メッシュの一覧情報（統合メッシュ毎の代表マテリアル番号）
	struct combined_bounding_mesh_info
	{
		static constexpr std::string_view extension = "ebmc";
		static constexpr std::uint32_t invalid_material = 0xFFFFFFFF;

		std::vector<std::uint32_t> material_indices;

		//	@tool 統合メッシュ本体のファイルパス（モデルパスの拡張子を置き換える）
		static std::filesystem::path make_mesh_path(std::string_view model_path, size_t combined_index);
		static std::filesystem::path make_info_path(std::string_view model_path);

		bool serialize(std::string_view file_path) const;
		bool deserialize(std::string_view file_path);
	};

	//	 model_resourceから cubic Signed Distance Field を生成
	bool create_cubic_sdf(cubic_sdf& out_sdf, model_resource_ptr resource, size_t index, const generate_bounding_mesh_data& mesh_data, uint32_t resolution = 32, float bounds_padding_rate = 0.1f);
}	//	namespace collision
