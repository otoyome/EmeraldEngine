//	頂点カラー編集用のマテリアルシェーダー
//	MATERIAL_SURFACE			: surface()
//	MATERIAL_SURFACE_OPACITY	: surface_opacity()
//	MATERIAL_GEOMETRY			: material_geometry()
#ifndef __EDIT_MODEL_COLOR_HLSL__
#define __EDIT_MODEL_COLOR_HLSL__

#include "model.hlsli"

#include "material_parameter_constant_buffer.hlsli"

//  マテリアル情報
cbuffer MATERIAL_CONSTANT_BUFFER
{
	float line_factor;
	float3 line_color;
};

#if defined(MATERIAL_SURFACE)
#include "gbuffer.hlsli"

//  面情報
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;
    data.base_color = float4(0, 0, 0, 1);
    data.emissive_color = pin.color[0].rgb;
    data.occlusion = 1.0f;
    data.specular = 0.5f;
    data.w_normal = normalize(pin.w_normal.xyz);
    data.w_position = pin.w_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.shading_model = material_shading_model;
    data.stencil = material_flags;
    return data;
}
#endif	//	defined(MATERIAL_SURFACE)

#if defined(MATERIAL_SURFACE_OPACITY)
#include "gbuffer.hlsli"

//	不透明度(depth_write / shadow_write パスのマスク処理用)
float surface_opacity(VS_OUT pin, bool is_front_face)
{
    return 1;
}
#endif	//	defined(MATERIAL_SURFACE_OPACITY)

//	深度と G-Buffer のパスだけで GS を使う
#if defined(MATERIAL_GEOMETRY) && (defined(SURFACE_PASS_DEPTH_WRITE) || defined(SURFACE_PASS_G_BUFFER))
#define MATERIAL_GEOMETRY_MAX_VERTEX_COUNT	3

//	面を法線方向へずらし、頂点カラーに線の色を掛けた面に置き換える
void material_geometry(VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
    int i;
    uint j;

    [unroll] for (i = 0; i < 3; i++)
    {
        VS_OUT element = (VS_OUT) 0;

        float3 v_normal = normalize(mul(float4(input[i].w_normal.xyz * line_factor, 0), view_transform).xyz);
        float3 v_position = mul(input[i].position, inverse_projection_transform).xyz;
        v_position += v_normal;	//	テキトーに

        element.position = mul(float4(v_position.xyz, 1), projection_transform);
#if defined(SURFACE_PASS_G_BUFFER)
        element.w_position = input[i].w_position;
        element.w_normal.xyz = input[i].w_normal.xyz;
        element.w_normal.w = 1.0f;
#if defined(HAS_TANGENT)
        element.w_tangent = input[i].w_tangent;
#endif  //  defined(HAS_TANGENT)
#else	//	defined(SURFACE_PASS_G_BUFFER)
        element.w_normal = input[i].w_normal;
#endif	//	defined(SURFACE_PASS_G_BUFFER)

#if VERTEX_COLOR_MAX > 0
        [unroll] for (j = 0; j < VertexColorMax; ++j)
        {
            element.color[j] = input[i].color[j] * float4(line_color.rgb, 1);
        }
#endif  //  VERTEX_COLOR_MAX > 0

#if defined(SURFACE_PASS_G_BUFFER)
#if TEXCOORD_MAX > 0
        [unroll] for (j = 0; j < TexcoordMax; ++j)
        {
            element.texcoord[j] = input[i].texcoord[j];
        }
#endif  //  TEXCOORD_MAX > 0
        element.current_clip_position = input[i].current_clip_position;
        element.previous_clip_position = input[i].previous_clip_position;
#endif	//	defined(SURFACE_PASS_G_BUFFER)
        output.Append(element);
    }

    output.RestartStrip();
}
#endif	//	defined(MATERIAL_GEOMETRY) && (defined(SURFACE_PASS_DEPTH_WRITE) || defined(SURFACE_PASS_G_BUFFER))

#endif	//	__EDIT_MODEL_COLOR_HLSL__
