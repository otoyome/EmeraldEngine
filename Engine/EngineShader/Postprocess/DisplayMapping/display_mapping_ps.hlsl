#include "fullscreen_quad.hlsli"
#include "lights.hlsli"
#include "scene_constant_buffer.hlsli"

//  ディスプレイマッピング
Texture2D color_map : register(t0);
SamplerState color_sampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = color_map.Sample(color_sampler, pin.texcoord);

    //  放射輝度 > 輝度に変換
    color.rgb *= luminous_efficacy;

    switch (output_curve)
    {
        case output_curve_srgb:
        {
            //  ディスプレイの最大輝度に合わせて変換
            color.rgb /= peak_nits;

            //  ディスプレイの色空間に移動
            color.rgb = convert_rcs_to_srgb(color.rgb);
            break;
        }
        case output_curve_st2084:
        {
            //  Rec2020線形色空間に変換
            color.rgb = convert_rcs_to_linear_rec2020(color.rgb);

            //  ST2084の基準輝度(10000nits)で正規化
            color.rgb /= st2084_max_luminance;

            //  ディスプレイの色空間に移動
            color.rgb = apply_st2084_curve(color.rgb);
            break;
        }
    }
    return float4(color.rgb, 1.0f);
}
