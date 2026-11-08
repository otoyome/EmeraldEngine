//	スプライトの頂点読込
//	stream0 : 座標(float3)
//	stream1 : 色(float4)
//	stream2 : UV(float2)
//	DrawIndexed の SV_VertexID はインデックスの値そのもの(BaseVertexLocation は足されない)なので、
//	C++ 側でインデックスに頂点の先頭位置を足しておく
#ifndef __SPRITE_VERTEX_HLSLI__
#define __SPRITE_VERTEX_HLSLI__

#include "vertex_stream.hlsli"

struct SPRITE_VERTEX
{
    float3 position;
    float4 color;
    float2 texcoord;
};

SPRITE_VERTEX load_sprite_vertex(uint vertex_id)
{
    SPRITE_VERTEX vertex;
    vertex.position = asfloat(vertex_stream0.Load3(vertex_id * 12));
    vertex.color = asfloat(vertex_stream1.Load4(vertex_id * 16));
    vertex.texcoord = asfloat(vertex_stream2.Load2(vertex_id * 8));
    return vertex;
}

#endif  //  __SPRITE_VERTEX_HLSLI__
