//  ライトをクラスターに割り当てる
//	2パス処理なのでコンパイル時に定義
//  CLUSTERED_FILL 無：書き込み開始位置を決めるため、クラスターごとのライト数を数える
//  CLUSTERED_FILL 有：累積和で決まった位置にライト番号を書き込む
#include "scene_constant_buffer.hlsli"
#include "clustered_lighting.hlsli"

StructuredBuffer<light_data> clustered_lights : register(t0);
StructuredBuffer<cluster_aabb> cluster_bounds : register(t1);
#if defined(CLUSTERED_FILL)
StructuredBuffer<uint> cluster_offsets : register(t2);
RWStructuredBuffer<uint> cluster_cursors : register(u0);
RWStructuredBuffer<uint> cluster_light_indices : register(u1);
#else // defined(CLUSTERED_FILL)
RWStructuredBuffer<uint> cluster_counts : register(u0);
#endif // defined(CLUSTERED_FILL)

static const uint assign_thread_count = 64;
static const uint dispatch_width = 65535;

groupshared float4 shared_sphere;     // 視線空間の中心と半径
groupshared uint3 shared_cluster_min; // 
groupshared uint3 shared_cluster_max; // 
groupshared uint shared_valid;        // 

//  球と AABB の交差判定
bool intersect_sphere_aabb(float3 center, float radius, float3 bmin, float3 bmax)
{
    float3 d = max(max(bmin - center, 0.0f), center - bmax);
    return dot(d, d) <= radius * radius;
}

//  視線空間の点をUV座標に投影
float2 project_to_texcoord(float3 view_position)
{
    float4 clip = mul(float4(view_position, 1.0f), projection_transform);
    float2 ndc = clip.xy / clip.w;
    return float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
}

[numthreads(assign_thread_count, 1, 1)]
void main(uint3 group_id : SV_GroupID, uint group_index : SV_GroupIndex)
{
    uint light_index = group_id.x + group_id.y * dispatch_width;

    //  ライトが影響を与えるクラスターを算出
    if (group_index == 0)
    {
        shared_valid = 0;
        if (light_index < cluster_light_count)
        {
            //  ライトの判定
            light_data light = clustered_lights[light_index];
            float radius = 1.0f / light.inv_range;
            float3 center = mul(float4(light.position, 1.0f), view_transform).xyz;
            shared_sphere = float4(center, radius);

            //  深度クリップ
            float depth_min = center.z - radius;
            float depth_max = center.z + radius;
            if (depth_max >= camera_clip_distance.x)
            {
                //  影響させるクラスターの調査
                uint3 cluster_min = uint3(0, 0, get_cluster_slice(depth_min));
                uint3 cluster_max = uint3(cluster_count.xy - 1, get_cluster_slice(depth_max));
                bool visible = true;
                if (depth_min > camera_clip_distance.x)
                {
                    //  ライトのAABBを算出
                    float2 uv_min = 1.0e30f, uv_max = -1.0e30f;
                    [unroll]
                    for (uint corner = 0; corner < 8; ++corner)
                    {
                        float3 offset = float3((corner & 1) ? radius : -radius, (corner & 2) ? radius : -radius, (corner & 4) ? radius : -radius);
                        float2 uv = project_to_texcoord(center + offset);
                        uv_min = min(uv_min, uv);
                        uv_max = max(uv_max, uv);
                    }
                    if (any(uv_max < 0.0f) || any(uv_min > 1.0f))
                    {
                        visible = false;
                    }
                    else
                    {
                        //  UV座標をタイル番号に変換
                        float2 tile_min = saturate(uv_min) * cluster_screen_size / cluster_tile_size;
                        float2 tile_max = saturate(uv_max) * cluster_screen_size / cluster_tile_size;
                        cluster_min.xy = min((uint2) tile_min, cluster_count.xy - 1);
                        cluster_max.xy = min((uint2) tile_max, cluster_count.xy - 1);
                    }
                }

                if (visible)
                {
                    shared_cluster_min = cluster_min;
                    shared_cluster_max = cluster_max;
                    shared_valid = 1;
                }
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();
    if (shared_valid == 0)
        return;

    //  範囲内のクラスターをグループ内で分担して判定する
    uint3 cluster_min = shared_cluster_min;
    uint3 range = shared_cluster_max - cluster_min + 1;
    uint cluster_total = range.x * range.y * range.z;
    for (uint index = group_index; index < cluster_total; index += assign_thread_count)
    {
        uint3 cluster = cluster_min + uint3(index % range.x, (index / range.x) % range.y, index / (range.x * range.y));
        uint cluster_index = get_cluster_index(cluster);
        //  クラスターAABBとライトの球体判定
        cluster_aabb aabb = cluster_bounds[cluster_index];
        if (!intersect_sphere_aabb(shared_sphere.xyz, shared_sphere.w, aabb.bmin, aabb.bmax))
            continue;

#if defined(CLUSTERED_FILL)
        //  クラスター情報を描きこむ
        uint slot;
        InterlockedAdd(cluster_cursors[cluster_index], 1, slot);
        uint destination = cluster_offsets[cluster_index] + slot;
        if (destination < cluster_index_capacity)
            cluster_light_indices[destination] = light_index;
#else // defined(CLUSTERED_FILL)
        //  クラスターカウント
        InterlockedAdd(cluster_counts[cluster_index], 1);
#endif // defined(CLUSTERED_FILL)
    }
}
