#include "debug_draw_line.hlsli"
#include "vertex_stream.hlsli"

//	stream0 : À•W(float3)‚ÆF(float4)‚ğ’¸“_‚²‚Æ‚É‚Ü‚Æ‚ß‚Ä•À‚×‚½‚à‚Ì
static const uint debug_line_vertex_stride = 28;

VS_OUT main(uint vertex_id : SV_VertexID)
{
    uint address = vertex_id * debug_line_vertex_stride;
    VS_OUT vout;
    vout.position = mul(float4(asfloat(vertex_stream0.Load3(address)), 1.0f), view_projection_transform);
    vout.color = asfloat(vertex_stream0.Load4(address + 12));
    return vout;
}
