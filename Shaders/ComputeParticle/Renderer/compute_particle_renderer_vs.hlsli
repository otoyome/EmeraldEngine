#include "compute_particle_renderer.hlsli"

PS_IN main(VS_IN vin)
{
    PS_IN vout;
    vout.position = mul(float4(vin.position, 1), view_projection_transform);
    vout.color = vin.color;
    vout.w_position = vin.position;
    vout.texcoord = vin.texcoord;

    vout.current_clip_position = vout.position;
    vout.previous_clip_position = mul(float4(vin.position, 1), previous_view_projection_transform);

    return vout;
}