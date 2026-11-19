#include "lights.hlsli"
#include "scene_constant_buffer.hlsli"

#include "color_space.hlsli"

cbuffer TONE_MAPPING_CONSTANT_BUFFER : register(b1)
{
    uint2 buffer_size;              //  バッファサイズ
    float2 tone_mapping_dummy;
};

RWTexture2D<float4> scene_color_map : register(u0);

static const uint2 threads = uint2(16, 16);

////////////////////////////////////////////////////////////////////
//  ホワイトバランス

//  色温度から白のXYZを求める
float3 get_white_point_xyz(float temperature)
{
    //  uv = 色温度の計算に使う色度座標(CIE 1960 UCS)
    float t = clamp(temperature, 1000.0f, 15000.0f);
    float u = (0.860117757f + 1.54118254e-4f * t + 1.28641212e-7f * t * t) / (1.0f + 8.42420235e-4f * t + 7.08145163e-7f * t * t);
    float v = (0.317398726f + 4.22806245e-5f * t + 4.20481691e-8f * t * t) / (1.0f - 2.89741816e-5f * t + 1.61456053e-7f * t * t);

    //  uv > xy
    float d = 2.0f * u - 8.0f * v + 4.0f;
    float2 xy = float2(3.0f * u, 2.0f * v) / d;

    //  xy > XYZ
    return convert_xyY_to_xyz(float3(xy, 1.0f));
}

//  指定した色温度の白を描画色空間の白に合わせる
float3 apply_white_balance(float3 rgb, float white_temperature)
{
    float3 rendering_white_xyz = convert_rcs_to_xyz(float3(1.0f, 1.0f, 1.0f));

    //  XYZ から錐体応答(LMS)への変換
    static const float3x3 xyz_to_lms_bradford =
    {
         0.8951f,  0.2664f, -0.1614f,
        -0.7502f,  1.7135f,  0.0367f,
         0.0389f, -0.0685f,  1.0296f,
    };

    float3 source_lms = mul(xyz_to_lms_bradford, get_white_point_xyz(white_temperature));
    float3 target_lms = mul(xyz_to_lms_bradford, rendering_white_xyz / rendering_white_xyz.y);

    float3 xyz = convert_rcs_to_xyz(rgb);
    float3 lms = mul(xyz_to_lms_bradford, xyz) * (target_lms / source_lms);

    //  LMSから XYZへ変換
    static const float3x3 lms_to_xyz_bradford =
    {
         0.9869929f, -0.1470543f,  0.1599627f,
         0.4323053f,  0.5183603f,  0.0492912f,
        -0.0085287f,  0.0400428f,  0.9684867f,
    };
    float3 balanced_xyz = mul(lms_to_xyz_bradford, lms);

    //  補正の前後で輝度を変えない
    balanced_xyz *= xyz.y / max(balanced_xyz.y, 1e-6f);
    return convert_xyz_to_rcs(balanced_xyz);
}

////////////////////////////////////////////////////////////////////
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
    float exposed_luminance = max(convert_rcs_to_luminance(exposed_radiance * luminous_efficacy), 1e-5f);

    //  ディスプレイの最大輝度で正規化した輝度をトーンマッピングし、表示する絶対輝度に戻す
    float tone_mapped_luminance = reinhard(exposed_luminance / peak_nits);
    float display_output_luminance = tone_mapped_luminance * peak_nits;

    //  色相・彩度が変わらないよう、rgb ごとではなく輝度の比で放射輝度を縮める
    return exposed_radiance * (display_output_luminance / exposed_luminance);
}

////////////////////////////////////////////////////////////////////
[numthreads(threads.x, threads.y, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (any(dispatch_thread_id.xy >= buffer_size))
        return;

    float4 color = scene_color_map[dispatch_thread_id.xy];

    //  ホワイトバランス
    float white_temperature = 5500.0f;
    color.rgb = apply_white_balance(color.rgb, white_temperature);

    //  トーンマッピング
    color.rgb = reinhard_tone_mapping(color.rgb);
    scene_color_map[dispatch_thread_id.xy] = color;
}
