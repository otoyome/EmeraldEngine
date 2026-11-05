//	3D モデルのマテリアルが使えない時に表示するマテリアルシェーダー
//	MATERIAL_SURFACE			: surface()
//	MATERIAL_SURFACE_OPACITY	: surface_opacity()
#ifndef __3D_FAILED_HLSL__
#define __3D_FAILED_HLSL__

#include "model.hlsli"

#include "material_parameter_constant_buffer.hlsli"

#if defined(MATERIAL_SURFACE)
#include "gbuffer.hlsli"
#include "shading_functions.hlsli"

//  面情報
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;

    data.base_color = float4(0.5f, 0, 0.5f, 1);
    data.roughness = 1;
    data.w_normal = normalize(pin.w_normal.xyz) * (is_front_face ? +1 : -1);
    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.stencil = material_flags;

    data.shading_model = shading_model_unlit;
    return data;
}
#endif	//	defined(MATERIAL_SURFACE)

#if defined(MATERIAL_SURFACE_OPACITY)
#include "gbuffer.hlsli"

//	不透明度(depth_write / shadow_write パスのマスク処理用)
float surface_opacity(VS_OUT pin, bool is_front_face)
{
    return 1;
}
#endif	//	defined(MATERIAL_SURFACE_OPACITY)

#endif	//	__3D_FAILED_HLSL__
