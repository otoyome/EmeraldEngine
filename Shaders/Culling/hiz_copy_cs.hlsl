#include "hiz.hlsli"

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= dst_size.x || id.y >= dst_size.y)
		return;

	int2 sp = min(int2(id.xy), int2(src_size) - 1);
	dst[id.xy] = src_depth.Load(int3(sp, 0));
}
