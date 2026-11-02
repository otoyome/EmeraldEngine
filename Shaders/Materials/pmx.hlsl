//	PMX(MMD)のマテリアルシェーダー
//	MATERIAL_SURFACE			: surface()
//	MATERIAL_SURFACE_OPACITY	: surface_opacity()
//	MATERIAL_GEOMETRY			: material_geometry()
#ifndef __PMX_HLSL__
#define __PMX_HLSL__

#include "model.hlsli"


#include "material_parameter_constant_buffer.hlsli"

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseMap, 0)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, SphereMap, 1)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, RampMap, 2)

//  UV座標
static const uint BaseTexcoord = 0;

//  マテリアル情報
cbuffer TOON_MATERIAL_CONSTANT_BUFFER
{
    float4  base_color_factor;  //  ベースカラー
    float4  specular_color;     //  スペキュラカラー
    float4  edge_color;         //  輪郭線色

    float   outline_size;       //  輪郭線サイズ
    float   roughness;          //  粗さ(未使用)
    float   metalness;          //  金属質(未使用)
    float   threshold_offset;   //  閾値

};

#if defined(MATERIAL_SURFACE)
#include "gbuffer.hlsli"
#include "shading_functions.hlsli"
#include "forward_light_data.hlsli"

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

    data.roughness = roughness;
    data.metalness = metalness;

    bool is_outline_pixel = pin.w_normal.w > 0.5f;
    if (is_outline_pixel)
    {
        data.base_color.rgb = edge_color.rgb;
        data.roughness = 1.0f;
    }
    else
    {
        //  ベースカラー
        float4 base_color = base_color_factor;
        if (HAS_RESOURCE(BaseMap))
        {
            base_color *= TEXTURE2D_SAMPLE(BaseMap, pin.texcoord[BaseTexcoord]);
        }
        data.base_color = base_color;

        //  平行光源を一つだけ処理してライティング
        if (1 >= forward_only_light_counts.x)
        {
            light_data directional_light = forward_only_directional_light_data[0];
            float3 L = directional_light.direction.xyz;
            L = normalize(L);
            float2 ramp_texcoord = float2(0.5f, saturate(dot(L, data.w_normal.xyz) * 0.5f + threshold_offset));
            float3 ramp_color = 1.0f;
            if (HAS_RESOURCE(RampMap))
            {
                ramp_color = TEXTURE2D_SAMPLE(RampMap, ramp_texcoord).rgb;
            }
            data.base_color.rgb *= ramp_color;
        }
    }
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

//	深度と G-Buffer のパスだけで GS を使う
#if defined(MATERIAL_GEOMETRY) && (defined(SURFACE_PASS_DEPTH_WRITE) || defined(SURFACE_PASS_G_BUFFER))
#include "gbuffer.hlsli"

//	ジオメトリシェーダーの最大出力頂点数(表面 3 + 輪郭線 3)
#define MATERIAL_GEOMETRY_MAX_VERTEX_COUNT	6

//  輪郭線算出関数
float3 calculate_outline_position(float3 w_position, float3 w_normal)
{
    return w_position + w_normal * outline_size;
}

//	法線方向に押し出した背面法の輪郭線を追加する
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

    if (outline_size <= 0.0f)
        return;

    //  輪郭線処理
#if defined(SURFACE_PASS_G_BUFFER)
    [unroll]
#endif	//	defined(SURFACE_PASS_G_BUFFER)
    for (i = 2; i >= 0; i--)
    {
        VS_OUT element = input[i];
        float3 w_position = calculate_outline_position(input[i].w_position.xyz, input[i].w_normal.xyz);
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

#endif	//	__PMX_HLSL__
