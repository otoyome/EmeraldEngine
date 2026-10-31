#include "geometric_primitive.hlsli"
#include "geometric_primitive_vertex.hlsli"

VS_OUT main(uint vertex_id : SV_VertexID)
{
    VS_OUT vout;
    float4 position = load_geometric_primitive_position(vertex_id);
    vout.position = mul(position, mul(world, view_projection_transform));
    vout.texcoord = load_geometric_primitive_texcoord(vertex_id);
    return vout;
}
