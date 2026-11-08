#include "exposure.hlsli"

RWTexture2D<float4> scene_color_map : register(u0);

static const uint2 threads = uint2(16, 16);

[numthreads(threads.x, threads.y, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (any(dispatch_thread_id.xy >= buffer_size))
        return;

    float4 color = scene_color_map[dispatch_thread_id.xy];
    color.rgb *= exposure_scale;
    scene_color_map[dispatch_thread_id.xy] = color;
}
