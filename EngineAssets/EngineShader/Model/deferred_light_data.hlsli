#ifndef __DEFERRED_LIGHT_HLSLI__
#define __DEFERRED_LIGHT_HLSLI__

#include "lights.hlsli"

//  ŒõŒ¹î•ñ
cbuffer DEFERREDLIGHT_CONSTANT_BUFFER : register(b3)
{
    light_data deferred_light_data;
};

//  ƒ‰ƒCƒgí•Ê‚Ìæ“¾
uint get_light_kinds()
{
    return get_light_type(deferred_light_data.attribute);
}

#endif  //  __DEFERRED_LIGHT_HLSLI__
