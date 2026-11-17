#include "deferred_rendering.hlsli"
#include "deferred_light_probe_data.hlsli"

//  IBL用テクスチャ
TextureCube diffuse_iem : register(t33);
TextureCube specular_pmrem : register(t34);
Texture2D lut_ggx : register(t35);
SamplerState ibl_sampler_state : register(s1);

float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    GBufferData gbuffer_data = get_gbuffer_data(pin.texcoord);

    if (gbuffer_data.shading_model == shading_model_unlit ||
        IsGBufferFlags(gbuffer_data, material_attribute_indirect_lighting_off))
    {
        return float4(0, 0, 0, 1);
    }

    float3 total_diffuse = 0, total_specular = 0;
    {
        //  ライトプローブ
        light_probes light_probe = light_probe_data;

        IndirectLightingData lighting_data;
        lighting_data.diffuse_iem = diffuse_iem;
        lighting_data.specular_pmrem = specular_pmrem;
        lighting_data.lut_ggx = lut_ggx;
        lighting_data.ibl_sampler = ibl_sampler_state;
        lighting_data.color_adjust = light_probe.color.rgb * light_probe.color.a;
        lighting_data.camera_position = camera_position.xyz;
        IndirectLighting result = integrate_indirect_bxdf(gbuffer_data, lighting_data);

        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }
    return float4(total_diffuse + total_specular, 1);
}
