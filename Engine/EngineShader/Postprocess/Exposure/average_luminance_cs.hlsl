#include "exposure.hlsli"

Texture2D<float4> source_texture : register(t0);
RWStructuredBuffer<float> average_luminance_buffer : register(u0);

#define THREAD_X        (32)
#define THREAD_Y        (32)
#define THREAD_COUNT    (THREAD_X * THREAD_Y)

//  íÜâõèdì_ïΩãœë™åı
static const float center_weighted_circle_diameter_rate = 1.0f / 3.0f;
static const float center_weighted_circle_weight_rate = 0.75f;

groupshared float block_luminance[THREAD_COUNT];
groupshared float block_weight[THREAD_COUNT];

//  íÜâõÇÃâ~ÇÃì‡ë§ÇÃâÊëfÇÃèdÇ›
float calculate_center_weight(float circle_radius)
{
    float inner_area = 3.14159265f * circle_radius * circle_radius;
    float outer_area = max(float(buffer_size.x) * float(buffer_size.y) - inner_area, 1.0f);
    return center_weighted_circle_weight_rate / (1.0f - center_weighted_circle_weight_rate) * outer_area / inner_area;
}

[numthreads(THREAD_X, THREAD_Y, 1)]
void main(uint3 group_thread_id : SV_GroupThreadID, uint group_index : SV_GroupIndex)
{
    uint2 round_up_size = round_up_to_multiple(buffer_size, THREAD_X);
    uint2 loop_count = round_up_size / uint2(THREAD_X, THREAD_Y);
    uint2 loop_offset = group_thread_id.xy * loop_count;
    uint2 remain_count = (buffer_size > loop_offset) ? (buffer_size - loop_offset) : uint2(0, 0);
    loop_count = min(remain_count, loop_count);

    bool center_weighted = metering_mode == metering_mode_center_weighted;
    float2 center = float2(buffer_size) * 0.5f;
    float circle_radius = float(buffer_size.y) * center_weighted_circle_diameter_rate * 0.5f;
    float inner_weight = center_weighted ? calculate_center_weight(circle_radius) : 1.0f;

    //  êœï™
    float total_value = 0;
    float total_weight = 0;
    for (uint y = 0; y < loop_count.y; ++y)
    {
        for (uint x = 0; x < loop_count.x; ++x)
        {
            uint2 position = loop_offset + uint2(x, y);
            float2 offset = (float2(position) + 0.5f) - center;
            float weight = (center_weighted && dot(offset, offset) <= circle_radius * circle_radius) ? inner_weight : 1.0f;

            float3 color = source_texture[position].rgb;
            total_value += convert_rcs_to_luminance(color) * weight;
            total_weight += weight;
        }
    }
    block_luminance[group_index] = total_value;
    block_weight[group_index] = total_weight;

    GroupMemoryBarrierWithGroupSync();

    //  îºï™Ç∏Ç¬ë´ÇµçáÇÌÇπÇÈ(1Ç¬Ç∏Ç¬èáÇ…ë´Ç∑ÇÊÇËä€ÇﬂåÎç∑Ç™è¨Ç≥Ç¢)
    [unroll]
    for (uint stride = THREAD_COUNT / 2; stride > 0; stride >>= 1)
    {
        if (group_index < stride)
        {
            block_luminance[group_index] += block_luminance[group_index + stride];
            block_weight[group_index] += block_weight[group_index + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (group_index == 0)
    {
        //  ï`âÊÇÕï˙éÀó Ç»ÇÃÇ≈ÅAAPEX ÇÃéÆÇ…çáÇÌÇπÇƒë™åıó Ç÷ïœä∑Ç∑ÇÈ
        float weight = block_weight[0];
        average_luminance_buffer[0] = (weight > 0.0f) ? block_luminance[0] / weight * luminous_efficacy : 0.0f;
    }
}
