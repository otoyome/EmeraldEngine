//	glTF(PBR)のマテリアルシェーダー
//	MATERIAL_SURFACE			: surface()
//	MATERIAL_SURFACE_OPACITY	: surface_opacity()
#ifndef __GLTF_HLSL__
#define __GLTF_HLSL__

#include "model.hlsli"

#include "material_parameter_constant_buffer.hlsli"

//  テクスチャ類登録
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, BaseColorMap, 0)
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, RoughnessMetallicMap, 1);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, NormalMap, 2);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, EmissiveMap, 3);
REGISTER_SHADER_RESOURCE_VIEW(Texture2D, OcclusionMap, 4);

//  UV座標
static const uint BaseTexcoord = 0;

//  マテリアル情報
cbuffer GLTF_MATERIAL_CONSTANT_BUFFER
{
	float4  base_color_factor;		//	ベースカラー
	float3  emissive_color_factor;	//	自己発光ファクター
	float normal_map_scale;			//	法線マップのスケーリング値

	float roughness_factor;			//	粗さ
	float metallic_factor;			//	金属質
	float occlusion_strength;		//	光の遮蔽強度
	float specular;					//	鏡面反射率の調整値(0.5 で F0 = 0.04)

	float emissive_strength;		//	自己発光の強さ
	float3 gltf_material_dummy;
};

#if defined(MATERIAL_SURFACE)
#include "gbuffer.hlsli"

//  面情報
SurfaceData surface(VS_OUT pin, bool is_front_face)
{
    SurfaceData data = (SurfaceData) 0;

    //  ベースカラー
    data.base_color = base_color_factor;
    if (HAS_RESOURCE(BaseColorMap))
    {
        data.base_color *= TEXTURE2D_SAMPLE(BaseColorMap, pin.texcoord[BaseTexcoord]);
    }

    //	自己発光色を取得
    data.emissive_color = emissive_color_factor;
    if (HAS_RESOURCE(EmissiveMap))
    {
        data.emissive_color *= TEXTURE2D_SAMPLE(EmissiveMap, pin.texcoord[BaseTexcoord]).rgb;
    }
    else
    {
        data.emissive_color *= data.base_color;
    }

    //  自己発光色を放射量へ変換
    data.emissive_color = convert_emissive_to_radiance(data.emissive_color * emissive_strength);

    //	法線/従法線/接線
    float3 N = normalize(pin.w_normal.xyz);
    bool has_tangent = (primitive_headers[pin.primitive_index].has_element & (1u << VERTEX_ELEMENT_TANGENT)) != 0;
    float3 T = has_tangent ? normalize(pin.w_tangent.xyz) : float3(1.0f, 0.0f, 0.0f);
    float sigma = has_tangent ? pin.w_tangent.w : 1.0f; //  sigmaはUVの方向による係数
    T = normalize(T - N * dot(N, T)); //  N と正確に直交するように調整
    float3 B = normalize(cross(N, T) * sigma);
    if (!is_front_face)
    {
        T = -T;
        B = -B;
        N = -N;
    }
    //	法線マッピング
    if (HAS_RESOURCE(NormalMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(NormalMap, pin.texcoord[BaseTexcoord]);
        float3 normal_factor = (sampled.xyz * 2.0f) - 1.0f;
        normal_factor = normalize(normal_factor * float3(normal_map_scale, normal_map_scale, 1.0f));
        N = normalize((normal_factor.x * T) + (normal_factor.y * B) + (normal_factor.z * N));
    }
    data.w_normal = N.xyz;
    data.w_tangent = T;
    data.bitangent_sign = is_front_face ? sigma : -sigma;

	//	金属質/粗さを取得
    data.roughness = saturate(roughness_factor);
    data.metalness = saturate(metallic_factor);
    if (HAS_RESOURCE(RoughnessMetallicMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(RoughnessMetallicMap, pin.texcoord[BaseTexcoord]);
        data.roughness *= sampled.g;
        data.metalness *= sampled.b;
    }

	//	光の遮蔽値を取得
    float occlusion_factor = 1.0f;
    if (HAS_RESOURCE(OcclusionMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(OcclusionMap, pin.texcoord[BaseTexcoord]);
        occlusion_factor *= sampled.r;
    }

    //  遮蔽強度を適応してから格納する
    data.occlusion = lerp(1.0f, occlusion_factor, occlusion_strength);
    data.specular = saturate(specular);

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
    float alpha = base_color_factor.a;
    if (HAS_RESOURCE(BaseColorMap))
    {
        alpha *= TEXTURE2D_SAMPLE(BaseColorMap, pin.texcoord[BaseTexcoord]).a;
    }
    return alpha;
}
#endif	//	defined(MATERIAL_SURFACE_OPACITY)

#endif	//	__GLTF_HLSL__
