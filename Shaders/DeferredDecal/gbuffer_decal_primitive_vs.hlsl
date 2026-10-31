#include "gbuffer_decal.hlsli"
#include "geometric_primitive_vertex.hlsli"

VS_OUT main(uint vertex_id : SV_VertexID)
{
    VS_OUT vout;
    float4 position = load_geometric_primitive_position(vertex_id);
    vout.position = mul(position, mul(decal_transform, view_projection_transform));
    return vout;
}
