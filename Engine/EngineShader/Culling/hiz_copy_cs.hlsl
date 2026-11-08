#include "hiz.hlsli"

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= dst_size.x || id.y >= dst_size.y)
		return;

	//	Å‚à‹ß‚¢[“x‚ğæ‚é
	uint2 s_begin = (id.xy * src_size) / dst_size;
	uint2 s_end = min(((id.xy + 1) * src_size + dst_size - 1) / dst_size, src_size);

	float depth = depth_near_value;
	for (uint y = s_begin.y; y < s_end.y; ++y)
	{
		for (uint x = s_begin.x; x < s_end.x; ++x)
		{
			depth = min(depth, src_depth.Load(int3(x, y, 0)));
		}
	}
	dst[id.xy] = depth;
}
