#pragma once


#include "model_resource.h"

namespace model_resource_utility
{
#if defined(USE_IMGUI)
	//	スキンメッシュではないモデルを結合
	void combine_unskinned_meshes(model_resource& resource, math::int3 grid_divisions);

	//	メッシュ最適化
	void optimized_meshs(model_resource& resource);
	//	影用メッシュ生成
	void generate_shadow_meshes(model_resource& resource, bool regenerate_buffer = true);
	//	LOD メッシュ生成
	void generate_lod_meshs(model_resource& resource, int max_lod_level, bool regeneratge_buffer = true);
	//	全メッシュの LOD メッシュ破棄
	void clear_lod_meshs(model_resource& resource);
	//	全メッシュの影用メッシュ破棄
	void clear_shadow_meshes(model_resource& resource);
	//	メッシュ最適化
	void optimize_meshs(model_resource& resource);
	//	メッシュレット生成
	void generate_meshlets(model_resource& resource);
#endif	//	defined(USE_IMGUI)
}
