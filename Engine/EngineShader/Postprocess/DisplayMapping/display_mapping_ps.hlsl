#include "fullscreen_quad.hlsli"
#include "Common/color_space.hlsli"

//  ディスプレイマッピング
cbuffer DISPLAY_MAPPING_CONSTANT_BUFFER : register(b0)
{
    float4 rendering_to_output[3];  //  描画色域から出力色域への行列(行、xyz)
    float peak_nits;                //  ディスプレイの最大輝度[nit]
    float luminous_efficacy;        //  最大視感効率[lm/W]
    uint output_curve;              //  出力の伝達関数
    float display_mapping_dummy;
};

//  出力の伝達関数(display_mapping_renderer::output_curves と同じ値)
static const uint output_curve_srgb = 0;

Texture2D color_map : register(t0);
SamplerState color_sampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = color_map.Sample(color_sampler, pin.texcoord);

    //  放射輝度 > 輝度に変換し、ディスプレイの最大輝度に合わせる
    float3 luminance = color.rgb * luminous_efficacy / peak_nits;

    //  出力の色域へ移す
    float3 output = float3(dot(rendering_to_output[0].xyz, luminance), dot(rendering_to_output[1].xyz, luminance), dot(rendering_to_output[2].xyz, luminance));

    //  伝達関数をかける(HDR10 の PQ は後で追加する)
    output = convert_linear_to_srgb(saturate(output));
    return float4(output, 1.0f);
}
