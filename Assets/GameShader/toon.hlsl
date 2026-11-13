//	トゥーンシェーディングのマテリアルシェーダー
//	テンプレートが定義するマクロで、必要な関数だけを有効にする
//	MATERIAL_SURFACE			: surface()
//	MATERIAL_SURFACE_OPACITY	: surface_opacity()
//	MATERIAL_GEOMETRY			: material_geometry()
#ifndef __TOON_HLSL__
#define __TOON_HLSL__

#include "model.hlsli"

#include "material_parameter_constant_buffer.hlsli"

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseMap, 0)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, IlmMap, 1);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, SssMap, 2);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, DetailMap, 3);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, EmissiveMap, 4);
static const int texture_max = 5;

//  UV座標
static const uint BaseTexcoord = 0;
static const uint IlmTexcoord = 0;
static const uint SssTexcoord = 0;
static const uint DetailTexcoord = 1;
static const uint EmissiveTexcoord = 0;

//  マテリアル情報
cbuffer TOON_MATERIAL_CONSTANT_BUFFER
{
    float4  base_color;                     //  ベースカラー
    float4  sss_color;                      //  SSS乗算カラー
    float   roughness;                      //  粗さ
    float   metallic;                       //  金属質
    float   shade_threshold;                //  陰影閾値
    float   shade_offset;                   //  陰影オフセット
    float4  fake_ambient_color;             //  疑似的な環境光色

    //  背面法による輪郭線情報
    float   outline_thickness;              //  輪郭線の厚み
    float   outline_depth_offset_distance;  //  輪郭線の深度オフセット距離(固定値でも良いと思う)
};

//	基底の範囲で0～1を算出する関数
#define linear_step(a, b, d) saturate((d - a) / (b - a))

//  輪郭線算出関数
float3 calculate_outline_position(float3 w_position, float3 w_normal, float4 vertex_color)
{
    // r : 陰影判定の閾値に対するオフセット 1が標準で0だと必ず影になる
    // g : カメラからの輪郭線の距離に応じてどの程度膨張させるかの係数
    // b : 輪郭線のZオフセット値
    // a : 輪郭線の太さ係数。 0.5:標準、0で無し

    // 距離による基準サイズ算出
    float base_thickness = 0.0f;
    {
        // カメラからの距離に応じた膨張係数を算出
        float camera_to_vertex = distance(w_position, camera_position.xyz);
#if 01
        camera_to_vertex *= get_tan_fov_y();   // 視野角補正
        camera_to_vertex *= 1.0f + vertex_color.g;    // 頂点色補正
        base_thickness = outline_thickness * camera_to_vertex;
#else   // 一応下記のような方法もある
        float2  outline_thickness;              //  輪郭線の厚み
        float2  outline_scaling_range;          //  輪郭線のサイズを変異させる範囲
        float camera_factor = linear_step(outline_scaling_range.x, outline_scaling_range.y, camera_to_vertex);
        camera_factor *= get_tan_fov_y();   // 視野角補正
        camera_factor *= vertex_color.g;    // 頂点色補正
        base_thickness = lerp(outline_thickness.x, outline_thickness.y, camera_factor);
#endif
    }

    // 厚み算出
    float thickness = convert_cm_to_unit(base_thickness);
    {
        thickness *= (vertex_color.a * 2.0f);   // 頂点色補正
    }

    // 位置調整
    w_position += normalize(w_normal) * thickness;

    // Zオフセット
    float depth_offset = convert_cm_to_unit(outline_depth_offset_distance) * vertex_color.b;
    w_position += normalize(w_position - camera_position.xyz) * depth_offset;

    return w_position;
}

#if defined(MATERIAL_SURFACE)
#include "gbuffer.hlsli"
#include "shading_functions.hlsli"

//  面情報
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;

    bool is_outline_pixel = pin.w_normal.w > 0.5f;

    data.base_color = float4(0, 0, 0, 1);
    data.emissive_color = 0;
    data.roughness = 0.8f;
    data.metalness = 0;
    data.occlusion = 1.0f;
    data.specular = 0.0f;
    data.w_normal = normalize(pin.w_normal.xyz) * (is_front_face ? +1 : -1);
    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.stencil = material_flags;

    //  ベースカラー
    data.base_color = base_color;
    if (HAS_RESOURCE(BaseMap))
    {
        data.base_color *= TEXTURE2D_SAMPLE(BaseMap, pin.texcoord[BaseTexcoord]);
    }

    if (is_outline_pixel)
    {
        data.base_color.rgb = float3(0, 0, 0);
        data.roughness = 1.0f;
    }
    else
    {
        //  SSSカラー
        //  RGB : 陰色
        //  A : 空き
        data.custom_data0.rgb = sss_color.rgb;
        if (HAS_RESOURCE(SssMap))
        {
            data.custom_data0.rgb *= TEXTURE2D_SAMPLE(SssMap, pin.texcoord[SssTexcoord]).rgb;
        }
        //  環境光の色は陰部にのみ適応
        //data.shade_color.rgb = apply_light_modifications(data.shade_color.rgb, fake_ambient_color.rgb);
        data.custom_data0.rgb *= fake_ambient_color.rgb;

        //  ILMカラー
        //  R : ハイライト強度
        //  G : テクスチャベースでの焼き込み影
        //  B : ハイライトの入りやすさ
        //  A :
        data.custom_data1.rgb = float3(0, 1, 0);
        if (HAS_RESOURCE(IlmMap))
        {
            data.custom_data1.rgb = TEXTURE2D_SAMPLE(IlmMap, pin.texcoord[IlmTexcoord]).rgb;
        }

        data.roughness = roughness;
        data.metalness = metallic;

        //  詳細情報
        if (HAS_RESOURCE(DetailMap))
        {
            float4 detail_color = TEXTURE2D_SAMPLE(DetailMap, pin.texcoord[DetailTexcoord]);
            data.base_color.rgb *= detail_color.rgb;
            data.custom_data0.rgb *= detail_color.rgb;
        }

        //  シェーディングモデル側にパラメーター引き渡し
        data.custom_data1.a = saturate(shade_threshold);
        data.custom_data2.r = saturate(shade_offset);
        data.custom_data0.a = saturate(pin.color[0].r); // 赤色がシェーディング補正値なので格納
    }

    data.shading_model = is_outline_pixel ? shading_model_unlit : material_shading_model;
    return data;
}
#endif	//	defined(MATERIAL_SURFACE)

#if defined(MATERIAL_SURFACE_OPACITY)
#include "gbuffer.hlsli"

//	不透明度(depth_write / shadow_write パスのマスク処理用)
float surface_opacity(VS_OUT pin, bool is_front_face)
{
    //  ベースカラー
    float alpha = base_color.a;
    if (HAS_RESOURCE(BaseMap))
    {
        alpha *= TEXTURE2D_SAMPLE(BaseMap, pin.texcoord[BaseTexcoord]).a;
    }
    return alpha;
}
#endif	//	defined(MATERIAL_SURFACE_OPACITY)

//	深度と G-Buffer のパスだけで GS を使う
#if defined(MATERIAL_GEOMETRY) && (defined(SURFACE_PASS_DEPTH_WRITE) || defined(SURFACE_PASS_G_BUFFER))
//	ジオメトリシェーダーの最大出力頂点数(表面 3 + 輪郭線 3)
#define MATERIAL_GEOMETRY_MAX_VERTEX_COUNT	6

//	背面法の輪郭線を追加する(depth_write / g_buffer パスで使用)
void material_geometry(VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
    int i;

	//	表面登録
    [unroll] for (i = 0; i < 3; i++)
    {
#if defined(SURFACE_PASS_G_BUFFER)
        input[i].w_normal.w = 0.0f;	//	アウトライン判定用に数値を変更
#endif	//	defined(SURFACE_PASS_G_BUFFER)
        output.Append(input[i]);
    }
    output.RestartStrip();

    //  輪郭線処理
#if defined(SURFACE_PASS_G_BUFFER)
    [unroll]
#endif	//	defined(SURFACE_PASS_G_BUFFER)
    for (i = 2; i >= 0; i--)
    {
        VS_OUT element = input[i];
        float3 w_position = calculate_outline_position(input[i].w_position.xyz, input[i].w_tangent.xyz, input[i].color[0]);
        element.position = mul(float4(w_position.xyz, 1), view_projection_transform);
        element.w_position.xyz = w_position;
        element.w_position.w = input[i].w_position.w;
#if defined(SURFACE_PASS_G_BUFFER)
        element.w_normal.w = 1.0f;	//	アウトライン判定用に数値を変更
#endif	//	defined(SURFACE_PASS_G_BUFFER)
        output.Append(element);
    }

    output.RestartStrip();
}
#endif	//	defined(MATERIAL_GEOMETRY) && (defined(SURFACE_PASS_DEPTH_WRITE) || defined(SURFACE_PASS_G_BUFFER))

#endif	//	__TOON_HLSL__
