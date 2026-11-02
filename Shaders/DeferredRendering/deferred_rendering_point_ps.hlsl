#include "deferred_rendering.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    GBufferData data = get_gbuffer_data(pin.texcoord);

    float3 total_diffuse = (float3) 0, total_specular = (float3) 0;
    if (data.shading_model != shading_model_unlit)
    {
        light_data point_light = deferred_light_data;
        float3 L = data.w_position.xyz - point_light.position.xyz;
        float len = length(L);
        if (len * point_light.inv_range < 1.0f)
        {
            L /= len;

            float shading_attenuation = calc_light_distance_attenuation(len, point_light.inv_range);
            
            float3 shadowing_attenuation = 1;
            if (IsGBufferFlags(data, material_attribute_shadow_receiver))
            {
                if (deferred_light_data.attribute & light_flag_use_shadow)
                {
                    shadowing_attenuation = shadowing_process_point(data.w_position, normalize(data.w_normal), point_light.position.xyz).rgb;
                }
            }
            
            LightingData lighting_data;
            lighting_data.shading_attenuation = shading_attenuation;
            lighting_data.shadowing_attenuation = shadowing_attenuation;
            lighting_data.direction = L;
            lighting_data.color = point_light.color;
            lighting_data.camera_position = camera_position.xyz;
            DirectLighting result = integrate_bxdf(data, lighting_data);
            total_diffuse += result.diffuse;
            total_specular += result.specular;
        }
    }

    return float4(total_diffuse + total_specular, 1);
}
