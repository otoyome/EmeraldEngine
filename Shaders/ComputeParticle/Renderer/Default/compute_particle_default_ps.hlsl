#include "compute_particle_default.hlsli"

float4 main(PS_IN pin) : SV_TARGET0
{
    float4 color = pin.color;
    if (HAS_RESOURCE(BaseColorMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(BaseColorMap, linear_sampler_state, pin.texcoord);
        color *= sampled;
    }

    return color;
}
