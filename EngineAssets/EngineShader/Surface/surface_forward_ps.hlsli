//  VS_OUT
//  surface関数
//  を作る事

#include "gbuffer.hlsli"
#include "shading_models.hlsli"
#include "forward_light_data.hlsli"
#include "clustered_lighting_shading.hlsli"

float4 main(VS_OUT pin, bool is_front_face : SV_IsFrontFace) : SV_TARGET
{
    SurfaceData surface_data = surface(pin, is_front_face);

    //  ライティング無しの場合はそのまま色を出力
    if (surface_data.shading_model == shading_model_unlit)
    {
        float4 color = surface_data.base_color;
        color.rgb += surface_data.emissive_color.rgb;
        return color;
    }
    
    GBufferData data = convert_g_buffer_data(surface_data);

    uint index;
    float3 total_diffuse = (float3) 0, total_specular = (float3) 0;
    
    //  平行光源
    [unroll]
    for (index = 0; index < directional_light_count; ++index)
    {
        if (index >= forward_only_light_counts.x)
            break;

        light_data directional_light = forward_only_directional_light_data[index];
        float3 L = normalize(directional_light.direction.xyz);

        float shading_attenuation = 1.0f;
        float4 shadowing_attenuation = 1.0f;

        //  影処理
        if (shadow_map_count > 0 && IsGBufferFlags(data, material_attribute_shadow_receiver))
        {
            shadowing_attenuation = shadowing_process(data.w_position, view_transform);
        }

        LightingData lighting_data;
        lighting_data.shading_attenuation = shading_attenuation;
        lighting_data.shadowing_attenuation = shadowing_attenuation;
        lighting_data.direction = L;
        lighting_data.color = directional_light.color;
        lighting_data.camera_position = camera_position.xyz;
        lighting_data.local_light = false;
        DirectLighting result = integrate_bxdf(data, lighting_data);
        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }

    //  点光源
    [unroll]
    for (index = 0; index < point_light_count; ++index)
    {
        if (index >= forward_only_light_counts.y)
            break;
        
        light_data point_light = forward_only_point_light_data[index];
        float3 L = data.w_position.xyz - point_light.position.xyz;
        float len = length(L);
        if (!(len * point_light.inv_range < 1.0f))
            continue;

        L /= len;

        float shading_attenuation = calc_light_distance_attenuation(len, point_light.inv_range);

        LightingData lighting_data;
        lighting_data.shading_attenuation = shading_attenuation;
        lighting_data.shadowing_attenuation = 1;
        lighting_data.direction = L;
        lighting_data.color = point_light.color;
        lighting_data.camera_position = camera_position.xyz;
        lighting_data.local_light = true;
        DirectLighting result = integrate_bxdf(data, lighting_data);
        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }

    //  スポットライト
    [unroll]
    for (index = 0; index < spot_light_count; ++index)
    {
        if (index >= forward_only_light_counts.z)
            break;

        light_data spot_light = forward_only_spot_light_data[index];
        float3 L = data.w_position.xyz - spot_light.position.xyz;
        float len = length(L);
        if (!(len * spot_light.inv_range < 1.0f))
            continue;

        float shading_attenuation = calc_light_distance_attenuation(len, spot_light.inv_range);
        L /= len;
        shading_attenuation *= calc_spot_cone_attenuation(spot_light, L);

        LightingData lighting_data;
        lighting_data.shading_attenuation = shading_attenuation;
        lighting_data.shadowing_attenuation = 1;
        lighting_data.direction = L;
        lighting_data.color = spot_light.color;
        lighting_data.camera_position = camera_position.xyz;
        lighting_data.local_light = true;
        DirectLighting result = integrate_bxdf(data, lighting_data);
        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }

    //  クラスタードライティング対象の点光源/スポットライトを処理
    accumulate_clustered_lights(data, total_diffuse, total_specular);

    //  IBL
    if (!IsGBufferFlags(data, material_attribute_indirect_lighting_off))
    {
        IndirectLightingData lighting_data;
        lighting_data.diffuse_iem = forward_only_diffuse_iem;
        lighting_data.specular_pmrem = forward_only_specular_pmrem;
        lighting_data.lut_ggx = forward_only_lut_ggx;
        lighting_data.ibl_sampler = forward_only_ibl_sampler_state;
        lighting_data.color_adjust = forward_only_light_probe_data.color.rgb * forward_only_light_probe_data.color.a;
        lighting_data.camera_position = camera_position.xyz;
        IndirectLighting result = integrate_indirect_bxdf(data, lighting_data);

        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }

    //  色返却
    return float4(total_diffuse + total_specular, surface_data.base_color.a);
}

