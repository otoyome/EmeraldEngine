//	パーティクルの既定のマテリアルシェーダー
//	MATERIAL_SURFACE			: surface()
//	MATERIAL_SURFACE_OPACITY	: surface_opacity()
#ifndef __PARTICLE_DEFAULT_HLSL__
#define __PARTICLE_DEFAULT_HLSL__

#include "compute_particle_default_rendering.hlsli"

#if defined(MATERIAL_SURFACE)
#include "compute_particle_default_surface.hlsli"
#endif	//	defined(MATERIAL_SURFACE)

#if defined(MATERIAL_SURFACE_OPACITY)
#include "compute_particle_default_surface_alpha.hlsli"
#endif	//	defined(MATERIAL_SURFACE_OPACITY)

#endif	//	__PARTICLE_DEFAULT_HLSL__
