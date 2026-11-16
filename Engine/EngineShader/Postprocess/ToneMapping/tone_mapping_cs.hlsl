#include "lights.hlsli"
#include "scene_constant_buffer.hlsli"

cbuffer TONE_MAPPING_CONSTANT_BUFFER : register(b1)
{
    uint2 buffer_size;              //  バッファサイズ
    float2 tone_mapping_dummy;
};

RWTexture2D<float4> scene_color_map : register(u0);

static const uint2 threads = uint2(16, 16);

//  ラインハルトのトーンマッピング
float reinhard(float x)
{
    //  トーンカーブの白点
    static const float tone_mapping_white_point = 1.0f;
    return x / (1 + x) * (1 + tone_mapping_white_point) / tone_mapping_white_point;
}

float3 reinhard_tone_mapping(float3 exposed_radiance)
{
    //  放射輝度から輝度に変換
    float exposed_luminance = max(convert_rcs_to_luminance(exposed_radiance * luminous_efficacy, rendering_gamut), 1e-5f);

    //  ディスプレイの最大輝度で正規化した輝度をトーンマッピングし、表示する絶対輝度に戻す
    float tone_mapped_luminance = reinhard(exposed_luminance / peak_nits);
    float display_output_luminance = tone_mapped_luminance * peak_nits;

    //  色相・彩度が変わらないよう、rgb ごとではなく輝度の比で放射輝度を縮める
    return exposed_radiance * (display_output_luminance / exposed_luminance);
}

[numthreads(threads.x, threads.y, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (any(dispatch_thread_id.xy >= buffer_size))
        return;

    float4 color = scene_color_map[dispatch_thread_id.xy];
    color.rgb = reinhard_tone_mapping(color.rgb);
    scene_color_map[dispatch_thread_id.xy] = color;
}
