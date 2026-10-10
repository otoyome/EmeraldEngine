#ifndef __DEFERRED_LIGHT_HLSLI__
#define __DEFERRED_LIGHT_HLSLI__

#include "lights.hlsli"

//  ŒõŒ¹î•ñ
cbuffer DEFERREDLIGHT_CONSTANT_BUFFER : register(b3)
{
    compund_lights compund_light_data;
    uint light_attribute;
    float3 light_data_dummy;
};

//  ƒ‰ƒCƒgí•Ê‚Ìæ“¾
uint get_light_kinds()
{
    return get_light_type(asuint(compund_light_data.work_data[1].w));
}

directional_lights convert_directional_lights()
{
    directional_lights data;
    data.direction = compund_light_data.work_data[0].xyz;
    data.dummy = 0.0f;
    data.color = compund_light_data.work_data[1].xyz;
    data.attribute = asuint(compund_light_data.work_data[1].w);
    return data;
}

point_lights convert_point_lights()
{
    point_lights data;
    data.position = compund_light_data.work_data[0].xyz;
    data.inv_range = compund_light_data.work_data[0].w;
    data.color = compund_light_data.work_data[1].xyz;
    data.attribute = asuint(compund_light_data.work_data[1].w);
    return data;
}

spot_lights convert_spot_lights()
{
    spot_lights data;
    data.position = compund_light_data.work_data[0].xyz;
    data.inv_range = compund_light_data.work_data[0].w;
    data.color = compund_light_data.work_data[1].xyz;
    data.attribute = asuint(compund_light_data.work_data[1].w);
    data.direction = compund_light_data.work_data[2].xyz;
    data.cone = asuint(compund_light_data.work_data[2].w);
    return data;
}

#endif  //  __DEFERRED_LIGHT_HLSLI__
