#include "compute_particle_renderer.hlsli"
#include "compute_particle_vertex.hlsli"

PS_IN main(uint vertex_id : SV_VertexID)
{
    VS_IN vin = load_particle_vertex(vertex_id);

    PS_IN vout;
    vout.position = mul(float4(vin.position, 1), view_projection_transform);
    vout.color = vin.color;
    vout.w_position = vin.position;
    vout.texcoord = vin.texcoord;

    vout.current_clip_position = vout.position;
    vout.previous_clip_position = mul(float4(vin.position, 1), previous_view_projection_transform);

    return vout;
}
