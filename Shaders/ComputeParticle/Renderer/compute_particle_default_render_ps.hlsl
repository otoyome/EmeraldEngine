#include "compute_particle.hlsli"

Texture2D color_map : register(t0);
SamplerState sampler_states : register(s0);

float4 main(PS_IN pin) : SV_TARGET0
{
    float4 color = color_map.Sample(sampler_states, pin.texcoord) * pin.color;
    clip(color.a - 0.01f);
    return color;
}
