#ifndef __DEFERRED_LIGHT_HLSLI__
#define __DEFERRED_LIGHT_HLSLI__

#include "lights.hlsli"

//  光源情報
//	@tool パス層（定数バッファは b7-b11）
cbuffer DEFERREDLIGHT_CONSTANT_BUFFER : register(b8)
{
    light_data deferred_light_data;
};

//  ライト種別の取得
uint get_light_kinds()
{
    return get_light_type(deferred_light_data.attribute);
}

#endif  //  __DEFERRED_LIGHT_HLSLI__
