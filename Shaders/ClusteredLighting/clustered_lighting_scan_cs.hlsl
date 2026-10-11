#include "clustered_lighting.hlsli"

StructuredBuffer<uint> cluster_counts : register(t0);         //  クラスター毎のライト数
RWStructuredBuffer<uint> cluster_offsets : register(u0);      //  担当範囲バッファ
RWStructuredBuffer<uint> cluster_statistics : register(u1);   //  ライト番号の総数をCPUに伝える

static const uint scan_thread_count = 1024;

groupshared uint shared_sums[scan_thread_count];

[numthreads(scan_thread_count, 1, 1)]
void main(uint group_index : SV_GroupIndex)
{
    //  1スレッドが担当するクラスターを決める
    uint cluster_total = cluster_count.x * cluster_count.y * cluster_count.z;
    uint per_thread = (cluster_total + scan_thread_count - 1) / scan_thread_count;
    uint begin = min(group_index * per_thread, cluster_total);
    uint end = min(begin + per_thread, cluster_total);

    uint local_sum = 0;
    for (uint index = begin; index < end; ++index)
        local_sum += cluster_counts[index];

    //  ライト数の合計を算出
    shared_sums[group_index] = local_sum;
    GroupMemoryBarrierWithGroupSync();
    [unroll]
    for (uint offset = 1; offset < scan_thread_count; offset <<= 1)
    {
        uint value = (group_index >= offset) ? shared_sums[group_index - offset] : 0;
        GroupMemoryBarrierWithGroupSync();
        shared_sums[group_index] += value;
        GroupMemoryBarrierWithGroupSync();
    }

    //  担当範囲を描きこむ
    uint running = shared_sums[group_index] - local_sum;
    for (uint index2 = begin; index2 < end; ++index2)
    {
        cluster_offsets[index2] = running;
        running += cluster_counts[index2];
    }

    //  ライト総数を書きだす
    if (group_index == scan_thread_count - 1)
        cluster_statistics[0] = shared_sums[group_index];
}
