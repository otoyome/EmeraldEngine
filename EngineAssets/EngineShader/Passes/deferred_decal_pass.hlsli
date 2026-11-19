//	deferred_decal ドメインのパステンプレート(全パス・全ステージ共通の main)
//	パス     : DECAL_PASS_PRIMITIVE / DECAL_PASS_FULLSCREEN
//	ステージ : DECAL_STAGE_VERTEX / DECAL_STAGE_PIXEL
//	その他   : MATERIAL_DECAL(PS のみ。マテリアルは decal_ps() を定義する)
#ifndef __DEFERRED_DECAL_PASS_HLSLI__
#define __DEFERRED_DECAL_PASS_HLSLI__

#if defined(DECAL_STAGE_VERTEX)

#if defined(DECAL_PASS_PRIMITIVE)
#include "gbuffer_decal_primitive_vs.hlsl"
#elif defined(DECAL_PASS_FULLSCREEN)
#include "gbuffer_decal_fullscreen_vs.hlsl"
#else
#error "deferred_decal_pass.hlsli : DECAL_PASS_xxx is not defined"
#endif

#elif defined(DECAL_STAGE_PIXEL)

#if defined(DECAL_PASS_PRIMITIVE)
#include "gbuffer_decal_primitive_ps.hlsli"
#elif defined(DECAL_PASS_FULLSCREEN)
#include "gbuffer_decal_fullscreen_ps.hlsli"
#else
#error "deferred_decal_pass.hlsli : DECAL_PASS_xxx is not defined"
#endif

#else
#error "deferred_decal_pass.hlsli : DECAL_STAGE_xxx is not defined"
#endif

#endif	//	__DEFERRED_DECAL_PASS_HLSLI__
