#ifndef __EXPOSURE_HLSLI__
#define __EXPOSURE_HLSLI__

#include "lights.hlsli"
#include "scene_constant_buffer.hlsli"

//	露出用の定数バッファ
cbuffer EXPOSURE_CONSTANT_BUFFER : register(b1)
{
    uint2 buffer_size;              //  バッファサイズ
    float exposure_scale;           //  露光倍率
    uint metering_mode;             //  測光モード
};

//  測光モード(exposure_settings::metering_modes と同じ値)
static const uint metering_mode_average = 0;
static const uint metering_mode_center_weighted = 1;

uint2 round_up_to_multiple(uint2 value, uint multiple)
{
    return (value + multiple - 1) / multiple * multiple;
}

#endif  //  __EXPOSURE_HLSLI__
