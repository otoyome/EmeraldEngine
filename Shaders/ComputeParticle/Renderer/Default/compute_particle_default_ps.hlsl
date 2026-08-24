#include "compute_particle_default.hlsli"

float4 main(PS_IN pin) : SV_TARGET0
{
    float4 color = pin.color;
    if (HAS_RESOURCE(BaseColorMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(BaseColorMap, linear_sampler_state, pin.texcoord);
        color *= sampled;
    }

    //	マスク処理
    if (material_blend_mode == material_blend_mode_mask)
    {
        float alpha_threshold = material_alpha_clip_threshold;
        alpha_threshold = lerp(alpha_threshold, 1.0f, calculate_alpha_dithering(pin.position.xy));
        clip(color.a - alpha_threshold);
    }

    return color;
}
