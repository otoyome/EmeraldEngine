//	デカールの既定のマテリアルシェーダー
//	MATERIAL_DECAL	: decal_ps()
#ifndef __DECAL_DEFAULT_HLSL__
#define __DECAL_DEFAULT_HLSL__

#include "default_decal_rendering.hlsli"

#if defined(MATERIAL_DECAL)
#include "default_decal_data_ps.hlsli"
#endif	//	defined(MATERIAL_DECAL)

#endif	//	__DECAL_DEFAULT_HLSL__
