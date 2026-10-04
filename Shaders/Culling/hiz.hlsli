#ifndef __HIZ_HLSLI__
#define __HIZ_HLSLI__

#include "common.hlsli"

Texture2D<float> src_depth : register(t0);
RWTexture2D<float> dst : register(u0);

cbuffer HIZ_CONSTANT_BUFFER : register(b0)
{
	uint2 dst_size;
	uint2 src_size;
};

#endif	//	__HIZ_HLSLI__
