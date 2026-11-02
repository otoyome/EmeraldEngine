#include "fullscreen_quad.hlsli"
#include "shading_functions.hlsli"
#include "scene_constant_buffer.hlsli"
#include "filter_functions.hlsli"
#include "common_gbuffer_loader.hlsli"
#include "common.hlsli"
#include "common_post_process.hlsli"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float4 post_process(VS_OUT pin)
{
    return float4(0.7f, 0.0f, 0.6f, length(pin.texcoord * 2 - 1));
}

