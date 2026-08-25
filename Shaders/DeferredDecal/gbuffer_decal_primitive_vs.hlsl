#include "gbuffer_decal.hlsli"

VS_OUT main(float4 position : POSITION, float2 texcoord : TEXCOORD)
{
    VS_OUT vout;
    vout.position = mul(position, mul(decal_transform, view_projection_transform));
    return vout;
}
