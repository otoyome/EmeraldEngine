#include "deferred_rendering.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    GBufferData data = get_gbuffer_data(pin.texcoord);

    float3 total_diffuse = (float3) 0, total_specular = (float3) 0;
    if (data.shading_model != shading_model_unlit)
    {
        light_data directional_light = deferred_light_data;
        float3 L = normalize(directional_light.direction.xyz);

        float shading_attenuation = 1.0f;
        float4 shadowing_attenuation = 1.0f;
        
        //  影処理
        if (IsGBufferFlags(data, material_attribute_shadow_receiver))
        {
            if (deferred_light_data.attribute & light_flag_use_shadow)
            {
                shadowing_attenuation = shadowing_process(data.w_position, view_transform);
            }
        }

        LightingData lighting_data;
        lighting_data.shading_attenuation = shading_attenuation;
        lighting_data.shadowing_attenuation = shadowing_attenuation.rgb;
        lighting_data.direction = L;
        lighting_data.color = directional_light.color;
        lighting_data.camera_position = camera_position.xyz;
        DirectLighting result = integrate_bxdf(data, lighting_data);
        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }
    return float4(total_diffuse + total_specular, 1);
}
