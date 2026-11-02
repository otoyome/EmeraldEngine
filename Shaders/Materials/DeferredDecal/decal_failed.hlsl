//	デカールのマテリアルが使えない時に表示するマテリアルシェーダー
//	MATERIAL_DECAL	: decal_ps()
#ifndef __DECAL_FAILED_HLSL__
#define __DECAL_FAILED_HLSL__

#include "failed_decal_rendering.hlsli"

#if defined(MATERIAL_DECAL)
#include "failed_decal_data_ps.hlsli"
#endif	//	defined(MATERIAL_DECAL)

#endif	//	__DECAL_FAILED_HLSL__
