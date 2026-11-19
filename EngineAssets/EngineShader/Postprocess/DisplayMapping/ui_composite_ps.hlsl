#include "fullscreen_quad.hlsli"
#include "color_space.hlsli"

Texture2D ui_map : register(t0);

cbuffer UI_COMPOSITE_CONSTANT_BUFFER : register(b0)
{
    float sdr_white_nits;       //  SDR の白の輝度[nit]
    uint output_curve;          //  出力カーブ
    float2 ui_composite_dummy;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    //  SDRで描いたUIとImGuiをバックバッファへ書き込む(HDR 出力のときはPQに変える)
    //  UI のテクスチャは sRGB の値が乗算済みアルファで入っている(透明で空にしてから半透明合成で描いたもの)
    float4 color = ui_map.Load(int3(pin.position.xy, 0));
    if (color.a <= 0.0f)
        discard;

    //  SDR 出力ならそのまま
    if (output_curve != output_curve_st2084)
        return color;

    //  sRGB からリニアの Rec.2020 へ
    float3 rgb = convert_srgb_to_linear(saturate(color.rgb / color.a));
    rgb = convert_linear_rec709_to_linear_rec2020(rgb);

    //  SDR の白の輝度に合わせて PQ にする
    rgb = apply_st2084_curve(rgb * sdr_white_nits / st2084_max_luminance);

    //  乗算済みアルファで返す
    return float4(rgb * color.a, color.a);
}
