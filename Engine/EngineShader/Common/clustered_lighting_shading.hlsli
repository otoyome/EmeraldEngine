#ifndef __CLUSTERED_LIGHTING_SHADING_HLSLI__
#define __CLUSTERED_LIGHTING_SHADING_HLSLI__

#include "clustered_lighting.hlsli"

StructuredBuffer<light_data> clustered_lights : register(t40);
StructuredBuffer<uint> cluster_counts : register(t41);
StructuredBuffer<uint> cluster_offsets : register(t42);
StructuredBuffer<uint> cluster_light_indices : register(t43);


//  クラスター内のライト数取得
uint get_clustered_light_count(float2 texcoord, float view_z)
{
    if (cluster_light_count == 0)
        return 0;

    uint cluster_index = get_cluster_index(get_cluster(texcoord, view_z));
    uint offset = cluster_offsets[cluster_index];
    uint count = cluster_counts[cluster_index];
    return (offset < cluster_index_capacity) ? min(count, cluster_index_capacity - offset) : 0;
}

//  ライト数をヒートマップの色にする
float3 get_clustered_heatmap_color(uint count)
{
    if (count == 0)
        return float3(0.0f, 0.0f, 0.0f);

    float t = saturate((float) count / cluster_visualize_max);
    return saturate(float3(1.5f - abs(4.0f * t - 3.0f), 1.5f - abs(4.0f * t - 2.0f), 1.5f - abs(4.0f * t - 1.0f)));
}

//  クラスター内のライトでライティング
void accumulate_clustered_lights(GBufferData data, float2 texcoord, float view_z, inout float3 total_diffuse, inout float3 total_specular)
{
    if (cluster_light_count == 0)
        return;

    //  ピクセルが属するクラスターのライトリスト
    uint cluster_index = get_cluster_index(get_cluster(texcoord, view_z));
    uint offset = cluster_offsets[cluster_index];
    uint count = cluster_counts[cluster_index];
    count = (offset < cluster_index_capacity) ? min(count, cluster_index_capacity - offset) : 0;

    for (uint index = 0; index < count; ++index)
    {
        light_data light = clustered_lights[cluster_light_indices[offset + index]];
        float3 L = data.w_position.xyz - light.position;
        float len = length(L);
        if (!(len * light.inv_range < 1.0f))
            continue;

        L /= len;
        float shading_attenuation = calc_light_distance_attenuation(len, light.inv_range);
        if (get_light_type(light.attribute) == light_type_spot)
            shading_attenuation *= calc_spot_cone_attenuation(light, L);

        LightingData lighting_data;
        lighting_data.shading_attenuation = shading_attenuation;
        lighting_data.shadowing_attenuation = 1;
        lighting_data.direction = L;
        lighting_data.color = light.color;
        lighting_data.camera_position = camera_position.xyz;
        DirectLighting result = integrate_bxdf(data, lighting_data);
        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }
}

//  ワールド座標からクラスターを求めて積算する
void accumulate_clustered_lights(GBufferData data, inout float3 total_diffuse, inout float3 total_specular)
{
    float4 clip = mul(float4(data.w_position.xyz, 1.0f), view_projection_transform);
    float2 ndc = clip.xy / clip.w;
    float2 texcoord = float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
    float view_z = mul(float4(data.w_position.xyz, 1.0f), view_transform).z;
    accumulate_clustered_lights(data, texcoord, view_z, total_diffuse, total_specular);
}

#endif  //  __CLUSTERED_LIGHTING_SHADING_HLSLI__
