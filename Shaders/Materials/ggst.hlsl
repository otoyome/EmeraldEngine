//	GGST 風トゥーンのマテリアルシェーダー
//	MATERIAL_SURFACE			: surface()
//	MATERIAL_SURFACE_OPACITY	: surface_opacity()
//	MATERIAL_GEOMETRY			: material_geometry()
#ifndef __GGST_HLSL__
#define __GGST_HLSL__

#include "model.hlsli"


#include "material_parameter_constant_buffer.hlsli"

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseMap, 0)

//  UV座標
static const uint BaseTexcoord = 0;

//  マテリアル情報
cbuffer TOON_MATERIAL_CONSTANT_BUFFER
{
    float4  base_color_factor;              //  ベースカラー
    float  roughness;
    float  metalness;

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

    return w_position;
}

#if defined(MATERIAL_SURFACE)
#include "gbuffer.hlsli"
#include "shading_functions.hlsli"

//  面情報
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;

    data.base_color = float4(0, 0, 0, 1);
    data.shade_color = 0;
    data.emissive_color = 0;
    data.occlusion_factor = 0;
    data.occlusion_strength = 0;
    data.unique_parameter = 0;
    data.w_normal = normalize(pin.w_normal.xyz) * (is_front_face ? +1 : -1);
    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.stencil = material_flags;

    //  ベースカラー
    data.base_color = base_color_factor;
    if (HAS_RESOURCE(BaseMap))
    {
        data.base_color *= TEXTURE2D_SAMPLE(BaseMap, pin.texcoord[BaseTexcoord]);
    }

    data.roughness = roughness;
    data.metalness = metalness;

    data.shading_model = material_shading_model;
    return data;
}
#endif	//	defined(MATERIAL_SURFACE)

#if defined(MATERIAL_SURFACE_OPACITY)
#include "gbuffer.hlsli"

//	不透明度(depth_write / shadow_write パスのマスク処理用)
float surface_opacity(VS_OUT pin, bool is_front_face)
{
    //  ベースカラー
    float alpha = base_color_factor.a;
    if (HAS_RESOURCE(BaseMap))
    {
        alpha *= TEXTURE2D_SAMPLE(BaseMap, pin.texcoord[BaseTexcoord]).a;
    }
    return alpha;
}
#endif	//	defined(MATERIAL_SURFACE_OPACITY)

//	@tool 深度と G-Buffer のパスだけで GS を使う(定義があるパスでは自動で GS が付く)
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

#endif	//	__GGST_HLSL__
