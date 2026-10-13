#ifndef __COMPUTE_PARTICLES_DEFAULT_SURFACE_ALPHA_HLSLI__
#define __COMPUTE_PARTICLES_DEFAULT_SURFACE_ALPHA_HLSLI__

//  VS_OUT (= PS_IN) を define してからインクルードする事
#include "compute_particle_default_rendering.hlsli"

//  面情報(α値のみ)
float surface(VS_OUT pin, bool is_front_face)
{
    float4 color = pin.color;
    if (HAS_RESOURCE(BaseColorMap))
    {
        color *= TEXTURE2D_SAMPLE(BaseColorMap, GET_SAMPLER_STATE_INDEX(BaseColorMap), pin.texcoord);
    }
    return color.a;
}

#endif  //  __COMPUTE_PARTICLES_DEFAULT_SURFACE_ALPHA_HLSLI__
