#include "common.hlsli"
#include "scene_constant_buffer.hlsli"
#include "clustered_lighting.hlsli"

RWStructuredBuffer<cluster_aabb> cluster_bounds : register(u0);

//  NDC深度の取得
float get_projection_depth(float view_z)
{
    float4 clip = mul(float4(0.0f, 0.0f, view_z, 1.0f), projection_transform);
    return clip.z / clip.w;
}

//  NDCを視線空間に戻す
float3 unproject(float2 ndc, float ndc_z)
{
    float4 view = mul(float4(ndc, ndc_z, 1.0f), inverse_projection_transform);
    return view.xyz / view.w;
}

//  2点を通る直線上で視線空間の奥行きが view_z になる点
float3 point_at_depth(float3 a, float3 b, float view_z)
{
    float t = (view_z - a.z) / (b.z - a.z);
    return a + (b - a) * t;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint total = cluster_count.x * cluster_count.y * cluster_count.z;
    if (dispatch_id.x >= total)
        return;

    //  クラスター
    uint3 cluster;
    cluster.x = dispatch_id.x % cluster_count.x;
    cluster.y = (dispatch_id.x / cluster_count.x) % cluster_count.y;
    cluster.z = dispatch_id.x / (cluster_count.x * cluster_count.y);

    //  画面タイルの範囲算出
    float2 uv_min = (float2) cluster.xy * cluster_tile_size / cluster_screen_size;
    float2 uv_max = min((float2) (cluster.xy + 1) * cluster_tile_size / cluster_screen_size, 1.0f);
    float2 ndc_min = float2(uv_min.x * 2.0f - 1.0f, 1.0f - uv_max.y * 2.0f);
    float2 ndc_max = float2(uv_max.x * 2.0f - 1.0f, 1.0f - uv_min.y * 2.0f);

    //  奥行き範囲算出
    float depth_min = (cluster.z == 0) ? min(camera_clip_distance.x, cluster_near) : get_cluster_slice_depth(cluster.z);
    float depth_max = (cluster.z == cluster_count.z - 1) ? gbuffer_max_linear_depth : get_cluster_slice_depth(cluster.z + 1);

    //  AABBの各コーナーを算出
    float near_depth = get_projection_depth(cluster_near);
    float far_depth = get_projection_depth(cluster_far);
    float3 bmin = 1.0e30f, bmax = -1.0e30f;
    [unroll]
    for (uint corner = 0; corner < 4; ++corner)
    {
        float2 ndc = float2((corner & 1) ? ndc_max.x : ndc_min.x, (corner & 2) ? ndc_max.y : ndc_min.y);
        float3 a = unproject(ndc, near_depth);
        float3 b = unproject(ndc, far_depth);
        float3 p0 = point_at_depth(a, b, depth_min);
        float3 p1 = point_at_depth(a, b, depth_max);
        bmin = min(bmin, min(p0, p1));
        bmax = max(bmax, max(p0, p1));
    }

    //  AABBに纏める
    cluster_aabb aabb;
    aabb.bmin = bmin;
    aabb.dummy0 = 0.0f;
    aabb.bmax = bmax;
    aabb.dummy1 = 0.0f;
    cluster_bounds[dispatch_id.x] = aabb;
}
