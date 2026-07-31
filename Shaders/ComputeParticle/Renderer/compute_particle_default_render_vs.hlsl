#include "compute_particle.hlsli"

PS_IN main(VS_IN vin)
{
    PS_IN vout;
    vout.position = mul(float4(vin.position, 1), view_projection_transform);
    vout.color = vin.color;
    vout.texcoord = vin.texcoord;
    return vout;
}