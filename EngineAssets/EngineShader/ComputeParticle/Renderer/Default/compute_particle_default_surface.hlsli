#ifndef __COMPUTE_PARTICLES_DEFAULT_SURFACE_HLSLI__
#define __COMPUTE_PARTICLES_DEFAULT_SURFACE_HLSLI__

//  VS_OUT (= PS_IN) を define してからインクルードする事
#include "compute_particle_default_rendering.hlsli"
#include "surface.hlsli"    //  SurfaceData / calculate_uvspace_velocity

//  面情報
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;

    //  ベースカラー
    float4 color = pin.color;
    if (HAS_RESOURCE(BaseColorMap))
    {
        color *= TEXTURE2D_SAMPLE(BaseColorMap, pin.texcoord);
    }
    data.base_color = color;
    data.emissive_color = (float3) 0;

    //  Unlitの時は自己発光に色を送る
    if (material_shading_model == shading_model_unlit)
    {
        data.emissive_color = data.base_color.rgb;
        data.base_color.rgb = (float3) 0;
    }

    //	自己発光は露出に合わせて放射量へ変換
    data.emissive_color = convert_particle_emission_to_radiance(data.emissive_color);

    //  ビルボードなので法線はカメラ方向を向ける
    data.w_normal = normalize(camera_position.xyz - pin.w_position.xyz);
    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);

    //  パーティクルはマップを持たないので既定値
    data.roughness = 1.0f;
    data.metalness = 0.0f;
    data.occlusion = 1.0f;
    data.specular = 0.5f;

    data.shading_model = material_shading_model;
    data.stencil = material_flags;
    return data;
}

#endif  //  __COMPUTE_PARTICLES_DEFAULT_SURFACE_HLSLI__
