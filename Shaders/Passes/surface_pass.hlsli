//	surface ドメインのパステンプレート(全パス・全ステージ共通の main)
//	マテリアルシェーダーの末尾で include する
//	material_shader_map が次の define を渡してマテリアルシェーダーをコンパイルする
//	パス     : SURFACE_PASS_DEPTH_WRITE / SURFACE_PASS_SHADOW_WRITE / SURFACE_PASS_G_BUFFER / SURFACE_PASS_FORWARD
//	ステージ : SURFACE_STAGE_VERTEX / SURFACE_STAGE_GEOMETRY / SURFACE_STAGE_PIXEL
//	その他   : VS_OUT、MATERIAL_SURFACE / MATERIAL_SURFACE_OPACITY / MATERIAL_GEOMETRY、FORWARD_RENDERING
#ifndef __SURFACE_PASS_HLSLI__
#define __SURFACE_PASS_HLSLI__

#if defined(SURFACE_STAGE_VERTEX)

#if defined(SURFACE_PASS_DEPTH_WRITE)
#include "model_depth_vs.hlsli"
#elif defined(SURFACE_PASS_SHADOW_WRITE)
#include "model_cascade_shadow_map_vs.hlsli"
#elif defined(SURFACE_PASS_G_BUFFER) || defined(SURFACE_PASS_FORWARD)
#include "model_vs.hlsli"
#else
#error "surface_pass.hlsli : SURFACE_PASS_xxx is not defined"
#endif

#elif defined(SURFACE_STAGE_GEOMETRY)

//	@tool 影のパスでは VS_OUT の render_target_array_index(カスケード)を書き換えずに出力すること
[maxvertexcount(MATERIAL_GEOMETRY_MAX_VERTEX_COUNT)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
    material_geometry(input, output);
}

#elif defined(SURFACE_STAGE_PIXEL)

#if defined(SURFACE_PASS_DEPTH_WRITE) || defined(SURFACE_PASS_SHADOW_WRITE)
#include "surface_depth_write_ps.hlsli"
#elif defined(SURFACE_PASS_G_BUFFER)
#include "surface_gbuffer_ps.hlsli"
#elif defined(SURFACE_PASS_FORWARD)
#include "surface_forward_ps.hlsli"
#else
#error "surface_pass.hlsli : SURFACE_PASS_xxx is not defined"
#endif

#else
#error "surface_pass.hlsli : SURFACE_STAGE_xxx is not defined"
#endif

#endif	//	__SURFACE_PASS_HLSLI__
