#include "clustered_lighting.hlsli"

Texture2D<float4> gbuffer_normal_depth : register(t0);
RWStructuredBuffer<uint> tile_max_slices : register(u0);

static const uint depth_thread_size = 16;

groupshared uint shared_tile_index;
groupshared uint shared_max_slice;

[numthreads(depth_thread_size, depth_thread_size, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID, uint group_index : SV_GroupIndex)
{
    uint width, height;
    gbuffer_normal_depth.GetDimensions(width, height);

    //  ピクセルが属するクラスターのタイルと奥行き分割
    bool inside = dispatch_id.x < width && dispatch_id.y < height;
    float2 texcoord = ((float2) dispatch_id.xy + 0.5f) / float2(width, height);
    uint3 cluster = uint3(0, 0, 0);
    if (inside)
    {
        float linear_depth = gbuffer_normal_depth.Load(int3(dispatch_id.xy, 0)).a;
        cluster = get_cluster(texcoord, linear_depth);
    }
    uint tile_index = get_cluster_tile_index(cluster.xy);

    //  先頭スレッドのタイル分をまとめてから書き込む
    if (group_index == 0)
    {
        shared_tile_index = tile_index;
        shared_max_slice = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    if (inside)
    {
        if (tile_index == shared_tile_index)
        {
            InterlockedMax(shared_max_slice, cluster.z);
        }
        else
        {
            //  タイルの境界をまたぐ場合は直接書く
            InterlockedMax(tile_max_slices[tile_index], cluster.z);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (group_index == 0)
        InterlockedMax(tile_max_slices[shared_tile_index], shared_max_slice);
}
