#ifndef __SHADING_MODEL_HLSLI__
#define __SHADING_MODEL_HLSLI__

#include "gbuffer.hlsli"
#include "shading_functions.hlsli"

struct DirectLighting
{
    float3 diffuse;
    float3 specular;
};

struct LightingData
{
    float3 direction;               //  ライト方向
    float shading_attenuation;      //  シェーディングによる減衰値
    float3 color;                   //  ライトカラー
    float3 shadowing_attenuation;   //  シャドウイングによる減衰値
    float3 camera_position;         //  視点
    bool local_light;               //  ローカルライトか判定
};

struct IndirectLighting
{
    float3 diffuse;
    float3 specular;
};

struct IndirectLightingData
{
    TextureCube diffuse_iem;        //  拡散反射用 IBL
    TextureCube specular_pmrem;     //  鏡面反射用 IBL
    Texture2D lut_ggx;              //  GGX のルックアップテーブル
    SamplerState ibl_sampler;       //  IBL 参照時のサンプラーステート
    float3 color_adjust;            //  ライトプローブの色 x 強さ
    float3 camera_position;         //  視点
};

DirectLighting integrate_bxdf(in GBufferData gbuffer_data, in LightingData lighting_data)
{
    DirectLighting result = (DirectLighting)0;
    switch (gbuffer_data.shading_model)
    {
        case shading_model_unlit:
        {
            //  ライティングは行わないので何もしない
            break;
        }
        case shading_model_default_lit:
        {
            float3 albedo = gbuffer_data.base_color.rgb;
            float3 normal = normalize(gbuffer_data.w_normal);
            float3 view_vector = normalize(gbuffer_data.w_position.xyz - lighting_data.camera_position.xyz);
            float3 diffuse_reflectance = lerp(albedo.rgb, 0.0f, gbuffer_data.metalness);
            float3 fresnel0 = CalcF0(gbuffer_data.specular, albedo.rgb, gbuffer_data.metalness);
            DirectBRDF(diffuse_reflectance, fresnel0, normal, view_vector, lighting_data.direction,
                       lighting_data.color, gbuffer_data.roughness, result.diffuse, result.specular);
            result.diffuse *= lighting_data.shadowing_attenuation * lighting_data.shading_attenuation;
            result.specular *= lighting_data.shadowing_attenuation * lighting_data.shading_attenuation;
            break;
        }
        case shading_model_pmx_lit:
        {
            float3 bright_color = gbuffer_data.base_color;
            float3 shade_color  = gbuffer_data.custom_data0.rgb;
            float  threshold    = gbuffer_data.custom_data0.a;
            float3 spec_color   = gbuffer_data.custom_data1.rgb;
            float  spec_power   = max(gbuffer_data.custom_data1.a * 256.0f, 1.0f);

            float3 N = normalize(gbuffer_data.w_normal);
            float3 L = -normalize(lighting_data.direction);
            float3 V = -normalize(gbuffer_data.w_position - lighting_data.camera_position);
            float3 H = normalize(L + V);

            //  シェーディング
            if (lighting_data.local_light)
            {
                //  ローカルライトの時は陰色にのみ影響を強く与える
                float  n_dot_l = dot(N, L) * 0.5f + 0.5f;
                float  lit     = step(threshold, n_dot_l * lighting_data.shading_attenuation);
                float3 shade   = lerp(shade_color, 1.0f, lit * lighting_data.shadowing_attenuation);
                float3 shadow  = lighting_data.shadowing_attenuation;

                //  default_lit と同じ明るさの基準（Lambert の 1/π）にして露出と合わせる
                float irradiance = lighting_data.color * rcp(PI);
                result.diffuse = bright_color * irradiance * shade * shadow;

                result.diffuse = lighting_data.shadowing_attenuation;
                //  正規化 Blinn-Phong
                float n_dot_h = saturate(dot(N, H));
                float norm    = (spec_power + 8.0f) / (8.0f * PI);
                result.specular = spec_color * norm * pow(n_dot_h, spec_power)
                                * lighting_data.color * saturate(dot(N, L))
                                * lighting_data.shadowing_attenuation * lighting_data.shading_attenuation;
                                result.specular = 0;
            }
            else
            {
                //  平行光源がベースの色を作成
                float  n_dot_l = dot(N, L) * 0.5f + 0.5f;
                float  lit     = step(threshold, n_dot_l);
                float  shadow  = lighting_data.shadowing_attenuation;
                float3 shade   = lerp(shade_color, 1.0f, lit * shadow);

                //  default_lit と同じ明るさの基準にして露出と合わせる
                result.diffuse = bright_color * rcp(PI) * lighting_data.color * shade;

                //  正規化 Blinn-Phong
                float n_dot_h = saturate(dot(N, H));
                float norm    = (spec_power + 8.0f) / (8.0f * PI);
                result.specular = spec_color * norm * pow(n_dot_h, spec_power)
                                * lighting_data.color * saturate(dot(N, L))
                                * lighting_data.shadowing_attenuation * lighting_data.shading_attenuation;
                                result.specular = 0;
            }
            break;
        }
        case shading_model_toon_lit:
        {
            float3 base_color = gbuffer_data.base_color;
            //  custom_data0 : rgb = 陰色, a = 頂点カラーR
            //  custom_data1 : rgb = ILM, a = 陰影閾値
            //  custom_data2 : r = 陰影オフセット
            float3 shade_color = gbuffer_data.custom_data0.rgb;
            float3 ilm_color = gbuffer_data.custom_data1.rgb;
            float3 normal = normalize(gbuffer_data.w_normal);
            float3 lighting_direction = -normalize(lighting_data.direction);
            float3 view_vector = -normalize(gbuffer_data.w_position.xyz - lighting_data.camera_position.xyz);
            // 反射ベクトルベースでも良いが、動きが安定しやすいのでハーフベクトルにしておく
            float3 half_vector = normalize(lighting_direction + view_vector);

            //  r : 陰影判定の閾値に対するオフセット 1が標準で0だと必ず影になる
            float vertex_r  = gbuffer_data.custom_data0.a;

            float shading_threshold = gbuffer_data.custom_data1.a;
            float shading_offset = gbuffer_data.custom_data2.r;

            //  各種係数算出
            float n_dot_l   = saturate(dot(normal, lighting_direction));
            float n_dot_v   = saturate(dot(normal, view_vector));
            float n_dot_h   = saturate(dot(normal, half_vector));

            //	ベースカラー算出
            //base_color  = apply_light_modifications(base_color, lighting_data.color);
            base_color *= lighting_data.color;

            //  スペキュラ算出
            float3 specular_color;
            {
                float specular_power = 10.0f;
                //  ILMのR成分を元にベースカラーを調整して元の色見に近い色を生成。
                float3  base  = lerp((float3)1.0f, min(base_color.rgb, 1.0f), 0.9f) * ilm_color.r;
                //specular_color = apply_light_modifications(base, lighting_data.color, specular_power);
                specular_color = base * lighting_data.color * specular_power;
            }

            //  計算補正
            float modified_n_dot_l = n_dot_l;
            float modified_n_dot_h = n_dot_h;
            {
                //  明/影の判定に対して、シェーディング調整値(頂点カラー/ILM)を適応。
                //  ILMのG成分をシェーディング調整係数として、
                //  0.0 = 暗
                //  0.5 = 基準
                //  1.0 = 明
                //  として計算
                float factor = (n_dot_l + shading_offset) * vertex_r * 0.5f;
                modified_n_dot_l    = saturate(factor + ilm_color.g * 2.0f - 1.0f);

                //  スペキュラの出方をILMのB成分で調整。
                //  小さいほど鋭く、大きいほど緩やかに
                //modified_n_dot_h    = blend_burn(n_dot_h, ilm_color.b);
                modified_n_dot_h    = saturate_mediump((1.0f - (1.0f - n_dot_h) / ilm_color.b));
            }

            //	ベースカラーとハイライト合成
            float3  base_and_specular_color = 0.0f;
            {
                float specular_size = 0.2f;
                float factor = (modified_n_dot_h >= saturate(1.0f - specular_size));
                base_and_specular_color = lerp(base_color.rgb, specular_color, factor);
            }

            //	シェーディング判定
            float shading_factor = min(vertex_r >= shading_threshold, modified_n_dot_l >= shading_threshold);

            //  シャドウ判定
            float3 shadow_factor = lighting_data.shadowing_attenuation * shading_factor;

            //  拡散反射光
            result.diffuse = lerp(shade_color, base_and_specular_color, shadow_factor * lighting_data.shading_attenuation);
            break;
        }
    }

    return result;
}

IndirectLighting integrate_indirect_bxdf(in GBufferData gbuffer_data, in IndirectLightingData lighting_data)
{
    IndirectLighting result = (IndirectLighting) 0;

    float3 albedo = gbuffer_data.base_color.rgb;
    float3 N = normalize(gbuffer_data.w_normal);
    float3 V = normalize(gbuffer_data.w_position.xyz - lighting_data.camera_position.xyz);

    switch (gbuffer_data.shading_model)
    {
        case shading_model_unlit:
        {
            //  ライティングは行わないので何もしない
            break;
        }
        case shading_model_default_lit:
        {
            //  入射光のうち拡散反射になる割合
            float3 diffuse_reflectance = lerp(albedo.rgb, 0.0f, gbuffer_data.metalness);

            //  垂直反射時のフレネル反射率
            float3 F0 = CalcF0(gbuffer_data.specular, albedo.rgb, gbuffer_data.metalness);

            result.diffuse = DiffuseIBL(N, V, gbuffer_data.roughness, diffuse_reflectance, F0, lighting_data.diffuse_iem, lighting_data.ibl_sampler) * lighting_data.color_adjust;
            result.specular = SpecularIBL(N, V, gbuffer_data.roughness, F0, lighting_data.lut_ggx, lighting_data.specular_pmrem, lighting_data.ibl_sampler) * lighting_data.color_adjust;

            //  遮蔽
            result.diffuse *= gbuffer_data.occlusion;
            result.specular *= CalcSpecularOcclusion(saturate(dot(N, -V)), gbuffer_data.occlusion, gbuffer_data.roughness);
            break;
        }
        case shading_model_pmx_lit:
        {
            //  6軸の平均
            float3 irradiance = 0.0f;
            irradiance += lighting_data.diffuse_iem.Sample(lighting_data.ibl_sampler, float3(+1, 0, 0)).rgb;
            irradiance += lighting_data.diffuse_iem.Sample(lighting_data.ibl_sampler, float3(-1, 0, 0)).rgb;
            irradiance += lighting_data.diffuse_iem.Sample(lighting_data.ibl_sampler, float3(0, +1, 0)).rgb;
            irradiance += lighting_data.diffuse_iem.Sample(lighting_data.ibl_sampler, float3(0, -1, 0)).rgb;
            irradiance += lighting_data.diffuse_iem.Sample(lighting_data.ibl_sampler, float3(0, 0, +1)).rgb;
            irradiance += lighting_data.diffuse_iem.Sample(lighting_data.ibl_sampler, float3(0, 0, -1)).rgb;
            irradiance *= 1.0f / 6.0f;
            irradiance /= PI;

            result.diffuse = albedo.rgb * irradiance * lighting_data.color_adjust;
            result.specular = 0.0f; //  スフィアマップに任せる
            break;
        }
        case shading_model_toon_lit:
        {
            //  フェイク環境光を使うので計算しない
            break;
        }
    }

    return result;
}

float4 get_shading_model_color(in GBufferData gbuffer_data)
{
    float4 result = (float4) 0;
    switch (gbuffer_data.shading_model)
    {
        case shading_model_unlit:       return float4(0.0f, 0.0f, 0.0f, 1);
        case shading_model_default_lit: return float4(0.5f, 0.5f, 0.5f, 1);
        case shading_model_pmx_lit:     return float4(0.5f, 0.5f, 1.0f, 1);
        case shading_model_toon_lit:    return float4(0.5f, 1.0f, 0.5f, 1);
    }

    return result;
}

#endif // __SHADING_MODEL_HLSLI__
