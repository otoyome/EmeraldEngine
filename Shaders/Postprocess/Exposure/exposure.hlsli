#ifndef __EXPOSURE_HLSLI__
#define __EXPOSURE_HLSLI__

//	露出用の定数バッファ
cbuffer EXPOSURE_CONSTANT_BUFFER : register(b0)
{
    uint2 buffer_size;              //  バッファサイズ
    float exposure_scale;           //  露光倍率 × ディスプレイ最大輝度
    uint metering_mode;             //  測光モード
    float4 luminance_coefficients;  //  描画色域の輝度係数(xyz)
    float luminous_efficacy;        //  最大視感効率[lm/W]
    float3 exposure_dummy;
};

//  測光モード(exposure_settings::metering_modes と同じ値)
static const uint metering_mode_average = 0;
static const uint metering_mode_center_weighted = 1;

uint2 round_up_to_multiple(uint2 value, uint multiple)
{
    return (value + multiple - 1) / multiple * multiple;
}

#endif  //  __EXPOSURE_HLSLI__
