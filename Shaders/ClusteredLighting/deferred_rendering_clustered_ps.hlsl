//  @tool クラスタードライティング（影無しの点光源/スポットライトを1パスでまとめて処理する）
//  @tool CLUSTERED_VISUALIZE 有 : ライティングの代わりにクラスターごとのライト数をヒートマップで出力する
#include "deferred_rendering.hlsli"
#include "clustered_lighting_shading.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    GBufferData data = get_gbuffer_data(pin.texcoord);

#if defined(CLUSTERED_VISUALIZE)
    //  @tool ヒートマップ表示
    return float4(get_clustered_heatmap_color(get_clustered_light_count(pin.texcoord, data.linear_depth)), 1);
#else   //  defined(CLUSTERED_VISUALIZE)
    float3 total_diffuse = (float3) 0, total_specular = (float3) 0;
    if (data.shading_model != shading_model_unlit)
    {
        accumulate_clustered_lights(data, pin.texcoord, data.linear_depth, total_diffuse, total_specular);
    }

    return float4(total_diffuse + total_specular, 1);
#endif  //  defined(CLUSTERED_VISUALIZE)
}
