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
            float4 debug_shadow = float4(1, 1, 1, 6);
            if (IsGBufferFlags(data, material_attribute_shadow_receiver))
            {
                if (deferred_light_data.attribute & light_flag_use_shadow)
                {
                    debug_shadow = shadowing_process_point(data.w_position, normalize(data.w_normal), point_light.position.xyz); shadowing_attenuation = debug_shadow.rgb;
                }
            }

//	[エンジン改造:シェーディング]
            if (gbuffer_data.shading_model == shading_model_toon_lit)
            {
                //  トゥーンリットの場合はデフォルトでシェーディングさせる
                gbuffer_data.shading_model = shading_model_default_lit;
            }
//	[エンジン改造:シェーディング]
            
            LightingData lighting_data;
            lighting_data.shading_attenuation = shading_attenuation;
            lighting_data.shadowing_attenuation = shadowing_attenuation;
            lighting_data.direction = L;
            lighting_data.color = point_light.color;
            lighting_data.camera_position = camera_position.xyz;
            DirectLighting result = integrate_bxdf(data, lighting_data);
            static const float3 debug_face_colors[7] = { float3(1,0,0), float3(0,1,1), float3(0,1,0), float3(1,0,1), float3(0,0,1), float3(1,1,0), float3(1,1,1) };
            return float4(debug_face_colors[(uint)debug_shadow.w] * (0.1f + 0.9f * debug_shadow.r) * shading_attenuation, 1);
            total_diffuse += result.diffuse;
            total_specular += result.specular;
        }
    }

    return float4(total_diffuse + total_specular, 1);
}
