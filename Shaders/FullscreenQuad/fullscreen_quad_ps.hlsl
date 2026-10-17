#include "fullscreen_quad.hlsli"


//	@tool パス層（シェーダーリソースは t64-t95）
Texture2D<float4> color_map : register(t71);
SamplerState sampler_states : register(s0);


float4 main(PS_IN pin) : SV_TARGET
{
    return color_map.Sample(sampler_states, pin.texcoord);
}
