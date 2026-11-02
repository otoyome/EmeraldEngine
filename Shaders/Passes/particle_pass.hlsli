//	particle ドメインのパステンプレート(全パス・全ステージ共通の main)
//	パス     : PARTICLE_PASS_DEPTH_WRITE / PARTICLE_PASS_G_BUFFER / PARTICLE_PASS_FORWARD
//	ステージ : PARTICLE_STAGE_VERTEX / PARTICLE_STAGE_PIXEL
//	その他   : VS_OUT(= PS_IN)、MATERIAL_SURFACE / MATERIAL_SURFACE_OPACITY
//	@tool パーティクルは影を描かない
#ifndef __PARTICLE_PASS_HLSLI__
#define __PARTICLE_PASS_HLSLI__

#if defined(PARTICLE_STAGE_VERTEX)

#if defined(PARTICLE_PASS_DEPTH_WRITE) || defined(PARTICLE_PASS_G_BUFFER) || defined(PARTICLE_PASS_FORWARD)
#include "compute_particle_renderer_vs.hlsli"
#else
#error "particle_pass.hlsli : PARTICLE_PASS_xxx is not defined"
#endif

#elif defined(PARTICLE_STAGE_PIXEL)

#if defined(PARTICLE_PASS_DEPTH_WRITE)
#include "surface_depth_write_ps.hlsli"
#elif defined(PARTICLE_PASS_G_BUFFER)
#include "surface_gbuffer_ps.hlsli"
#elif defined(PARTICLE_PASS_FORWARD)
#include "surface_forward_ps.hlsli"
#else
#error "particle_pass.hlsli : PARTICLE_PASS_xxx is not defined"
#endif

#else
#error "particle_pass.hlsli : PARTICLE_STAGE_xxx is not defined"
#endif

#endif	//	__PARTICLE_PASS_HLSLI__
