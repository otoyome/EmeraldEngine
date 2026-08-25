#ifndef __GEOMETRIC_PRIMITIVE_OBJECT_DATA_HLSLI__
#define __GEOMETRIC_PRIMITIVE_OBJECT_DATA_HLSLI__

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

cbuffer GEOMETRIC_PRIMITIVE_CONSTANT_BUFFER : register(b1)
{
    row_major float4x4 world;
};

#endif // __GEOMETRIC_PRIMITIVE_OBJECT_DATA_HLSLI__
