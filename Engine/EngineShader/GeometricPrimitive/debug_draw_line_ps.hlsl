#include "debug_draw_line.hlsli"
#include "Common/color_space.hlsli"

cbuffer DEBUG_DRAW_COLOR_CONSTANT_BUFFER : register(b1)
{
    float4 srgb_to_scene[3];
};

float4 main(VS_OUT pin) : SV_TARGET
{
	float3 rgb709 = convert_srgb_to_linear(pin.color.rgb);
	float3 scene = float3(dot(srgb_to_scene[0].xyz, rgb709), dot(srgb_to_scene[1].xyz, rgb709), dot(srgb_to_scene[2].xyz, rgb709));
	return float4(scene, pin.color.a);
}
