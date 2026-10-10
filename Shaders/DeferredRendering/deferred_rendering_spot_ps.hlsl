#include "deferred_rendering.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    GBufferData data = get_gbuffer_data(pin.texcoord);

    float3 total_diffuse = (float3) 0, total_specular = (float3) 0;
    if (data.shading_model != shading_model_unlit)
    {
        spot_lights spot_light = convert_spot_lights();
        float3 L = data.w_position.xyz - spot_light.position.xyz;
        float len = length(L);
        if (len * spot_light.inv_range < 1.0f)
        {
            float shading_attenuation = calc_light_distance_attenuation(len, spot_light.inv_range);
            L /= len;
            shading_attenuation *= calc_spot_cone_attenuation(spot_light, L);

            float3 shadowing_attenuation = 1;
            if (IsGBufferFlags(data, material_attribute_shadow_receiver))
            {
                if (light_attribute & light_flag_use_shadow)
                {
                    shadowing_attenuation = shadowing_process(data.w_position, view_transform).rgb;
                }
            }
            
            LightingData lighting_data;
            lighting_data.shading_attenuation = shading_attenuation;
            lighting_data.shadowing_attenuation = shadowing_attenuation;
            lighting_data.direction = L;
            lighting_data.color = spot_light.color;
            lighting_data.camera_position = camera_position.xyz;
            DirectLighting result = integrate_bxdf(data, lighting_data);
            total_diffuse += result.diffuse;
            total_specular += result.specular;
        }
    }
    return float4(total_diffuse + total_specular, 1);
}
