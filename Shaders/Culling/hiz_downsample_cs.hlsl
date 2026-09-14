#include "hiz.hlsli"

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= dst_size.x || id.y >= dst_size.y)
		return;

	int2 s = int2(id.xy) * 2;
	int2 smax = int2(src_size) - 1;

	float d0 = src_depth.Load(int3(min(s + int2(0, 0), smax), 0));
	float d1 = src_depth.Load(int3(min(s + int2(1, 0), smax), 0));
	float d2 = src_depth.Load(int3(min(s + int2(0, 1), smax), 0));
	float d3 = src_depth.Load(int3(min(s + int2(1, 1), smax), 0));

	dst[id.xy] = max(max(d0, d1), max(d2, d3));
}
