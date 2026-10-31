//	ジオメトリックプリミティブの頂点読込
//	stream0 : 座標(float3)
//	stream1 : UV(float2)
//	DrawIndexed の SV_VertexID はインデックスの値になる
#ifndef __GEOMETRIC_PRIMITIVE_VERTEX_HLSLI__
#define __GEOMETRIC_PRIMITIVE_VERTEX_HLSLI__

#include "vertex_stream.hlsli"

float4 load_geometric_primitive_position(uint vertex_id)
{
    return float4(asfloat(vertex_stream0.Load3(vertex_id * 12)), 1.0f);
}

float2 load_geometric_primitive_texcoord(uint vertex_id)
{
    return asfloat(vertex_stream1.Load2(vertex_id * 8));
}

#endif  //  __GEOMETRIC_PRIMITIVE_VERTEX_HLSLI__
