#pragma once

#include "occlusion_culling_system.h"

//	メッシュ描画順リスト
struct render_mesh_indices
{
	static constexpr int forward_size = static_cast<int>(translucency_paths::max);
	std::vector<float> lengths;						//	距離リスト
	std::vector<std::size_t> opaque;				//	不透明描画リスト
	std::vector<std::size_t> mask;					//	マスク描画リスト
	std::vector<std::size_t> forward[forward_size];	//	半透明描画リスト
	void clear()
	{
		opaque.clear();
		mask.clear();
		lengths.clear();
		for (auto& indices : forward)
			indices.clear();
	}
};

//	パーティクル描画振り分けリスト
struct render_particle_indices
{
	static constexpr int forward_size = static_cast<int>(translucency_paths::max);
	std::vector<std::size_t> opaque;				//	不透明描画リスト
	std::vector<std::size_t> mask;					//	マスク描画リスト
	std::vector<std::size_t> forward[forward_size];	//	半透明描画リスト
	void clear()
	{
		opaque.clear();
		mask.clear();
		for (auto& indices : forward)
			indices.clear();
	}
};

//	デカール描画振り分けリスト
//	@remainder	時間が無いのでもう一律描画
struct render_decal_indices
{
	std::vector<std::size_t> draw_list;	//	描画順リスト
	void clear()
	{
		draw_list.clear();
	}
};

//	レンダラー用コンテキスト
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct renderer_context
{
	common_pipeline_states::scene_constants scene_constant;				//	シーン情報
	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;			//	シーン定数バッファ
	std::shared_ptr<render_data_context> render_data;					//	描画情報

	std::shared_ptr<render_mesh_indices> render_mesh_indices;			//	メッシュ描画データ
	std::shared_ptr<render_particle_indices> render_particle_indices;	//	パーティクル描画データ
	std::shared_ptr<render_decal_indices> render_decal_indices;			//	デカール描画データ

	std::shared_ptr<occlusion_culling_system> occlusion_culling_system;	//	オクルージョンカリング情報
	float occlusion_animation_distance;									//	オクルージョンカリングをアニメーションモデルも対象にする
};
