//  @tool クラスタードライティング（影無しの点光源/スポットライトを1パスでまとめて処理する）
#include "deferred_rendering.hlsli"
#include "clustered_lighting_shading.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    GBufferData data = get_gbuffer_data(pin.texcoord);

    float3 total_diffuse = (float3) 0, total_specular = (float3) 0;
    if (data.shading_model != shading_model_unlit)
    {
        accumulate_clustered_lights(data, pin.texcoord, data.linear_depth, total_diffuse, total_specular);
    }

    return float4(total_diffuse + total_specular, 1);
}
