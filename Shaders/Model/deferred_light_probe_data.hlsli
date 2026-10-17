#ifndef __DEFERRED_LIGHT_PROBE_HLSLI__
#define __DEFERRED_LIGHT_PROBE_HLSLI__

#include "lights.hlsli"

//  ライトプローブ情報
//	@tool パス層（定数バッファは b7-b11）
cbuffer DEFERRED_LIGHT_PROBE_CONSTANT_BUFFER : register(b10)
{
    light_probes light_probe_data;
    uint light_probe_attribute;
    float3 light_probe_dummy;
};

#endif  //  __DEFERRED_LIGHT_PROBE_HLSLI__
