#include "sprite.hlsli"

Texture2D color_map : register(t0);
SamplerState color_mapState : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
	return color_map.Sample(color_mapState, pin.texcoord) * pin.color;
}
