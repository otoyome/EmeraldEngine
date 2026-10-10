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
int get_light_kinds()
{
    return (int) (compund_light_data.work_data[3].w + 0.5f);
}

directional_lights convert_directional_lights()
{
    directional_lights data;
    data.direction = compund_light_data.work_data[0];
    data.color = compund_light_data.work_data[1];
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

light_probes convert_light_probes()
{
    light_probes light_probe;
    light_probe.position = compund_light_data.work_data[0];
    light_probe.color = compund_light_data.work_data[1];
    light_probe.inner_area = compund_light_data.work_data[2];
    light_probe.outer_area = compund_light_data.work_data[3];
    return light_probe;
}

#endif  //  __DEFERRED_LIGHT_HLSLI__
