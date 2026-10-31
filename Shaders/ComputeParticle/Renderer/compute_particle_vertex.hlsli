//	コンピュートパーティクルの頂点読込
//	stream0 : compute_particle_build_primitive_cs が書き出した頂点
//	座標 = float3
//	色 = float4
//	UV = float2
//	の順で 1 頂点 36 バイト
#ifndef __COMPUTE_PARTICLE_VERTEX_HLSLI__
#define __COMPUTE_PARTICLE_VERTEX_HLSLI__

#include "compute_particle_structs.hlsli"
#include "vertex_stream.hlsli"

VS_IN load_particle_vertex(uint vertex_id)
{
    uint address = vertex_id * vertex_size;
    VS_IN vin;
    vin.position = asfloat(vertex_stream0.Load3(address));
    vin.color = asfloat(vertex_stream0.Load4(address + 12));
    vin.texcoord = asfloat(vertex_stream0.Load2(address + 28));
    return vin;
}

#endif  //  __COMPUTE_PARTICLE_VERTEX_HLSLI__
