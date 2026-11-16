#ifndef __SURFACE_HLSLI__
#define __SURFACE_HLSLI__

//  サーフェイス計算結果と、それを GBufferData へ変換するヘルパー
#include "gbuffer.hlsli"
#include "lights.hlsli"

////////////////////////////////////////////////////////////////////////////////////////////////////
//  サーフェイス計算結果格納情報
struct SurfaceData
{
    float4 base_color; //  ベースカラー
    float3 emissive_color; //  自己発光
    uint shading_model; //  シェーディング方式
    float3 w_normal; //  ワールド法線
    float3 w_tangent; //  ワールド接線
    float bitangent_sign; //  従法線の向き
    float3 w_position; //  ワールド座標
    float2 velocity; //  速度
    float roughness; //  粗さ
    float metalness; //  金属質
    float occlusion; //  遮蔽値
    float specular; //  鏡面反射率の調整値
    uint stencil; //  ビットフラグ(下位 15bit のみ有効)

    //  シェーディングモデル毎に変わる変数
    float4 custom_data0;
    float4 custom_data1;
    float4 custom_data2;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  @tool 自己発光の単位(ライティングサンプルと同じ扱い)
//  @tool マテリアルの自己発光色 1.0 を emissive_reference_nits の輝度とし、描画で使う放射量へ変換してシーンに足す

//  @tool 自己発光色 1.0 に対応する輝度[nit]。glTF は自己発光に物理単位を定めていないので SDR の基準白相当にする
//  @tool シーンの輝度なので、ディスプレイの基準白(sdr_reference_white_nits)とは別の値として扱い、ディスプレイの設定で変えない
static const float emissive_reference_nits = 100.0f;

//  @tool マテリアルの自己発光色を放射量へ変換する
float3 convert_emissive_to_radiance(float3 emissive_color)
{
    return emissive_color * (emissive_reference_nits / luminous_efficacy);
}

GBufferData convert_g_buffer_data(in SurfaceData surface_data)
{
    GBufferData g_buffer_data = (GBufferData)0;
    g_buffer_data.base_color = surface_data.base_color.rgb;
    g_buffer_data.shading_model = surface_data.shading_model;
    g_buffer_data.w_normal = surface_data.w_normal;
    g_buffer_data.w_tangent = surface_data.w_tangent;
    g_buffer_data.bitangent_sign = surface_data.bitangent_sign;
    g_buffer_data.w_position = surface_data.w_position;
    g_buffer_data.velocity = surface_data.velocity;
    g_buffer_data.roughness = surface_data.roughness;
    g_buffer_data.metalness = surface_data.metalness;
    g_buffer_data.occlusion = surface_data.occlusion;
    g_buffer_data.specular = surface_data.specular;
    g_buffer_data.stencil = surface_data.stencil;

    g_buffer_data.custom_data0 = surface_data.custom_data0;
    g_buffer_data.custom_data1 = surface_data.custom_data1;
    g_buffer_data.custom_data2 = surface_data.custom_data2;

    return g_buffer_data;
}

#endif  //  __SURFACE_HLSLI__
