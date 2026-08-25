#ifndef __SHADOW_MAP_HLSLI__
#define __SHADOW_MAP_HLSLI__

static const uint shadow_map_max = 6;
static const uint shadow_map_constant_bias = 0.0001f;

//  シャドウマップ情報
cbuffer SHADOW_MAP_CONSTANT_BUFFER : register(b4)
{
    uint shadow_map_count;
    float3 shadow_attenuation;
    float4 shadow_bias[shadow_map_max];
    float4 shadow_split_distance[shadow_map_max];
    row_major float4x4 light_view_projection_transform[shadow_map_max];
    uint shadow_mask;
    float3 shadow_dummy;
};

//  シャドウマップ
Texture2DArray<float> shadow_maps : register(t10);
SamplerState shadow_maps_sampler_state : register(s10);

//	シャドウサンプル関数
float3 shadowmap_fetch(int index, float3 texcoord)
{
    float3 texcoordUVW = float3(texcoord.xy, index);
    float depth = shadow_maps.Sample(shadow_maps_sampler_state, texcoordUVW).r;
    float factor = step(texcoord.z, depth + shadow_map_constant_bias);
    return lerp(shadow_attenuation, float3(1.0f, 1.0f, 1.0f), factor);
}

//  PCFフィルタリングシャドウサンプル関数
float3 shadowmap_fetch_pcf(int index, float3 texcoord, float2x2 rotation_matrix, float radius = 3.0f)
{
    float3 tex_size;
    shadow_maps.GetDimensions(tex_size.x, tex_size.y, tex_size.z);

    //  PCFノイズ
    static const uint poisson_disk_size = 16;
    static const float2 poisson_disk[poisson_disk_size] =
    {
        float2(-0.94201624, -0.39906216),
        float2(0.94558609, -0.76890725),
        float2(-0.09418410, -0.92938870),
        float2(0.34495938, 0.29387760),
        float2(-0.91588581, 0.45771432),
        float2(-0.81544232, -0.87912464),
        float2(-0.38277543, 0.27676845),
        float2(0.97484398, 0.75648379),
        float2(0.44323325, -0.97511554),
        float2(0.53742981, -0.47373420),
        float2(-0.26496911, -0.41893023),
        float2(0.79197514, 0.19090188),
        float2(-0.24188840, 0.99706507),
        float2(-0.81409955, 0.91437590),
        float2(0.19984126, 0.78641367),
        float2(0.14383161, -0.14100790)
    };

    float3 attenuation = (float3) 0.0f;
    [unroll]
    for (uint poisson_index = 0; poisson_index < poisson_disk_size; ++poisson_index)
    {
        float2 offset = (mul(rotation_matrix, poisson_disk[poisson_index]) / tex_size.xy) * radius;

        float3 texcoordUVW = float3(texcoord.xy + offset, index);
        float depth = shadow_maps.SampleLevel(shadow_maps_sampler_state, texcoordUVW, 0).r;
        float value = step(texcoord.z, depth + shadow_map_constant_bias);
        attenuation += value.xxx;
    }
    return lerp(shadow_attenuation, float3(1.0f, 1.0f, 1.0f), attenuation / poisson_disk_size);
}

//	シャドウサンプル
float4 shadowing_process(float3 world_position, float4x4 view_matrix)
{
    float4 view_position = mul(float4(world_position, 1.0f), view_matrix);
    float view_depth = view_position.z / view_position.w;

    float angle = frac(sin(dot(world_position.xy, float2(12.9898f, 78.233f))) * 43758.5453f) * 3.14159265f * 2;
    float2x2 rot = { cos(angle), -sin(angle), sin(angle), cos(angle) };

    float3 light_attenuation = float3(1.0f, 1.0f, 1.0f);
    uint sm_index = shadow_map_max;
    for (uint index = 0; index < shadow_map_max; ++index)
    {
        if (index >= shadow_map_count)
            break;

        //	深度クリップ
        if (!(shadow_split_distance[index].x <= view_depth && view_depth <= shadow_split_distance[index].w))
            continue;

        //	シャドウ座標
        float4 shadow_position = mul(float4(world_position, 1.0f), light_view_projection_transform[index]);
        shadow_position /= shadow_position.w;

        //	境界クリップ
        float3 texcoord = float3(shadow_position.xy * float2(0.5f, -0.5f) + (float2)0.5f, shadow_position.z);
        if (!(all(texcoord >= (float3)0.0f) * all(texcoord <= (float3)1.0f)))
            continue;

        //light_attenuation = shadowmap_fetch(index, texcoord);
        light_attenuation = shadowmap_fetch_pcf(index, texcoord, rot, shadow_bias[index].w);
        sm_index = index;
        break;
    }

    return float4(light_attenuation, sm_index);
}

#endif  //  __SHADOW_MAP_HLSLI__
