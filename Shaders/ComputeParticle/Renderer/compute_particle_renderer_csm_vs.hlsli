#include "compute_particle_renderer.hlsli"
#include "compute_particle_vertex.hlsli"

VS_IN main(uint vertex_id : SV_VertexID)
{
    return load_particle_vertex(vertex_id);
}
