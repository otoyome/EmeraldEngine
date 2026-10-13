#ifndef __MODEL_DEPTH_VS_HLSLI__
#define __MODEL_DEPTH_VS_HLSLI__

//#include "model.hlsli"

//	メッシュレット頂点情報読込
VS_DEPTH_IN load_vertex_pulled(MeshletContext meshlet_context, uint vertex_id)
{
    VS_DEPTH_IN vin = (VS_DEPTH_IN)0;
    uint local_vertex_index = meshlet_resolve_local_vertex_index(meshlet_context, vertex_id);

    vin.position = meshlet_load_position(meshlet_context, local_vertex_index);
    vin.normal = meshlet_load_normal(meshlet_context, local_vertex_index);
#if defined(HAS_TANGENT)
    vin.tangent = meshlet_load_tangent(meshlet_context, local_vertex_index);
#endif  //  defined(HAS_TANGENT)
#if VERTEX_COLOR_MAX > 0
    vin.color[0] = meshlet_load_color0(meshlet_context, local_vertex_index);
#endif  //  VERTEX_COLOR_MAX > 0
#if TEXCOORD_MAX > 0
    vin.texcoord[0] = meshlet_load_texcoord0(meshlet_context, local_vertex_index);
#endif  //  TEXCOORD_MAX > 0
#if TEXCOORD_MAX > 1
    vin.texcoord[1] = meshlet_load_texcoord1(meshlet_context, local_vertex_index);
#endif  //  TEXCOORD_MAX > 1
#if BONE_MAX > 0
    vin.joints[0] = meshlet_load_joints0(meshlet_context, local_vertex_index);
    vin.weights[0] = meshlet_load_weights0(meshlet_context, local_vertex_index);
#endif  //  BONE_MAX > 0
#if BONE_MAX > 4
    vin.joints[1] = meshlet_load_joints1(meshlet_context, local_vertex_index);
    vin.weights[1] = meshlet_load_weights1(meshlet_context, local_vertex_index);
#endif  //  BONE_MAX > 4
#if BONE_MAX > 8
    vin.joints[2] = meshlet_load_joints2(meshlet_context, local_vertex_index);
    vin.weights[2] = meshlet_load_weights2(meshlet_context, local_vertex_index);
#endif  //  BONE_MAX > 8
#if BONE_MAX > 12
    vin.joints[3] = meshlet_load_joints3(meshlet_context, local_vertex_index);
    vin.weights[3] = meshlet_load_weights3(meshlet_context, local_vertex_index);
#endif  //  BONE_MAX > 12

    return vin;
}

VS_DEPTH_OUT main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    uint index;
    VS_DEPTH_OUT vout = (VS_DEPTH_OUT)0;

	//	メッシュレット構築
    bool is_degenerate;
    MeshletContext meshlet_context = meshlet_build_context(instance_id, vertex_id, is_degenerate);
    if (is_degenerate)
    {
        //  構築できない場合は縮退ポリゴン化
        vout.position = float4(0, 0, 0, 0);
        return vout;
    }
    VS_DEPTH_IN vin = load_vertex_pulled(meshlet_context, vertex_id);

#if defined(HAS_MORPH)  //  モーフィング対応
    uint local_vertex_index_for_morph = meshlet_resolve_local_vertex_index(meshlet_context, vertex_id);
    apply_morph_position(meshlet_context, vin.position, local_vertex_index_for_morph);
    apply_morph_normal(meshlet_context, vin.normal, local_vertex_index_for_morph);
#endif  //  defined(HAS_MORPH)

#if BONE_MAX > 0
    float3 position = 0, normal = 0;
#if defined(HAS_TANGENT)
    float3 tangent = 0;
#endif  //  defined(HAS_TANGENT)
    [unroll] for (index = 0; index < BoneMax; ++index)
    {
        uint bone_index = vin.joints[index / 4][index % 4];
        float bone_weight = vin.weights[index / 4][index % 4];
        position += mul(float4(vin.position, 1), get_bone_transform(meshlet_context, bone_index)).xyz * bone_weight;
        normal += mul(float4(vin.normal, 0), get_bone_transform(meshlet_context, bone_index)).xyz * bone_weight;
#if defined(HAS_TANGENT)
        tangent += mul(float4(vin.tangent.xyz, 0), get_bone_transform(meshlet_context, bone_index)).xyz * bone_weight;
#endif  //  defined(HAS_TANGENT)
    }
#else  //  BONE_MAX > 0

    float3 position = vin.position;
    float3 normal = vin.normal;
#if defined(HAS_TANGENT)
    float3 tangent = vin.tangent.xyz;
#endif  //  defined(HAS_TANGENT)

#endif  //  BONE_MAX > 0

    vout.position = mul(float4(position, 1), view_projection_transform);
    vout.w_position.xyz = position;
    vout.w_position.w = 1.0f;
    vout.w_normal.xyz = normalize(normal).xyz;
    vout.w_normal.w = 0.0f;
#if defined(HAS_TANGENT)
    vout.w_tangent.xyz = normalize(tangent).xyz;
    vout.w_tangent.w = vin.tangent.w;
#endif  //  defined(HAS_TANGENT)

#if VERTEX_COLOR_MAX > 0
    [unroll] for (index = 0; index < VertexColorMax; ++index)
    {
        vout.color[index] = vin.color[index];
    }
#endif  //  VERTEX_COLOR_MAX > 0

#if TEXCOORD_MAX > 0
    [unroll] for (index = 0; index < TexcoordMax; ++index)
    {
        vout.texcoord[index] = vin.texcoord[index];
    }
#endif  //  TEXCOORD_MAX > 0

    return vout;
}

#endif  //  __MODEL_DEPTH_VS_HLSLI__
