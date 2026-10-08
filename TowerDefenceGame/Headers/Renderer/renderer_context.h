#pragma once

#include "graphics/occlusion_culling_system.h"
#include "graphics/hiz_buffer.h"
#include "graphics/meshlet_culling_system.h"
#include "graphics/scene_view.h"
#include "graphics/scene_view_state.h"

//	メッシュ描画順序リスト
struct render_mesh_indices
{
	static constexpr int forward_size = static_cast<int>(translucency_paths::max);
	std::vector<float> lengths;						//	長さリスト
	std::vector<std::size_t> opaque;				//	不透明描画リスト
	std::vector<std::size_t> mask;					//	マスク描画リスト
	std::vector<std::size_t> forward[forward_size];	//	透明描画リスト
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
	std::vector<std::size_t> forward[forward_size];	//	透明描画リスト
	void clear()
	{
		opaque.clear();
		mask.clear();
		for (auto& indices : forward)
			indices.clear();
	}
};

//	デカール描画振り分けリスト
//	@remainder	数が少ないのですぐ一律描画
struct render_decal_indices
{
	std::vector<std::size_t> draw_list;	//	描画順序リスト
	void clear()
	{
		draw_list.clear();
	}
};

//	レンダラー用コンテキスト
struct renderer_context
{
	common_pipeline_states::scene_constants scene_constant;				//	シーン情報
	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;			//	シーン定数バッファ
	std::shared_ptr<render_data_context> render_data;					//	描画情報

	std::shared_ptr<render_mesh_indices> render_mesh_indices;			//	メッシュ描画データ
	std::shared_ptr<render_particle_indices> render_particle_indices;	//	パーティクル描画データ
	std::shared_ptr<render_decal_indices> render_decal_indices;			//	デカール描画データ

	scene_view_ptr current_view;										//	描画視点情報
	scene_view_state_ptr current_view_state;							//	描画制御情報
};
