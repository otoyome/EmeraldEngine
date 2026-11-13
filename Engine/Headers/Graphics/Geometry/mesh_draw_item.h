#pragma once

#include <unordered_map>
#include <vector>

#include "Graphics/Core/buffer_view.h"

struct pooled_meshlet;

//	描画バッファ情報
struct mesh_draw_item
{
	enum class flags
	{
		culling = 1 << 0,		//	カリングを行うかどうか
		static_mesh = 1 << 1,	//	アニメーションしないモデルかどうか
	};
	struct morph_mesh
	{
		const std::unordered_map<semantic, buffer_view>* vertex_buffer_views;
		float weight = 0;
	};
	struct sub_mesh
	{
		const std::unordered_map<semantic, buffer_view>* vertex_buffer_views;
		D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		std::vector<morph_mesh> morph_meshes;
		const std::vector<pooled_meshlet>* meshlets = nullptr;
	};

	UINT vertex_count;										//	Drawで使用する際の頂点数
	UINT system_flags = 0;									//	モデルフラグ類
	std::vector<sub_mesh> color_meshs;						//	色描画メッシュ
	sub_mesh shadow_mesh;									//	影用描画メッシュ
	math::float2 lod_distance_range = {0.0f, 10000.0f};		//	LOD距離範囲
	UINT unique_id = 0xFFFFFFFF;							//	ユニークID
	math::float3 bounds_min, bounds_max;					//	AABB範囲
	math::float4x4 world_transform = {};					//	ワールド変換行列
	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_buffer;	//	プリミティブバッファ

	//	LOD選択
	std::size_t select_lod_mesh_index(float mesh_length) const
	{
		std::size_t lod_count = color_meshs.size();
		std::size_t lod_mesh_index = 0;
		if (lod_count > 1)
		{
			float lod_range_min = lod_distance_range.x;
			float lod_range_max = lod_distance_range.y;
			float lod_range = lod_range_max - lod_range_min;
			float lod_rate = 0.0f;
			if (lod_range > 1e-6f)
			{
				lod_rate = (mesh_length - lod_range_min) / lod_range;
				if (lod_rate < 0.0f) lod_rate = 0.0f;
				if (lod_rate > 1.0f) lod_rate = 1.0f;
			}
			lod_mesh_index = static_cast<std::size_t>(lod_rate * static_cast<float>(lod_count - 1) + 0.5f);
			if (lod_mesh_index >= lod_count)
				lod_mesh_index = lod_count - 1;
		}
		return lod_mesh_index;
	}
};
