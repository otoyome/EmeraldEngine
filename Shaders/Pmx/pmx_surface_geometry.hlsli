#ifndef __PMX_SURFACE_GEOMETRY_HLSLI__
#define __PMX_SURFACE_GEOMETRY_HLSLI__

#include "pmx_rendering.hlsli"
#include "gbuffer.hlsli"

//  ó÷äsê¸éZèoä÷êî
float3 calculate_outline_position(float3 w_position, float3 w_normal)
{
    return w_position + w_normal * outline_size;
}

#endif	//	__PMX_SURFACE_GEOMETRY_HLSLI__
