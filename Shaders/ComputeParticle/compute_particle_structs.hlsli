#ifndef __COMPUTE_PARTICLES_STRUCTS_HLSLI__
#define __COMPUTE_PARTICLES_STRUCTS_HLSLI__

////////////////////////////////////////////////////////////////////////////////////////////////////////
struct VS_IN
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};
static const uint index_size = 6;
static const uint vertex_size = 4 * 9;

struct PS_IN
{
    float4 position : SV_POSITION;
    float3 w_position : WORLD_POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;

    float4 current_clip_position : CLIP_POSITION0;
    float4 previous_clip_position : CLIP_POSITION1;
};

#endif  //  __COMPUTE_PARTICLES_STRUCTS_HLSLI__
