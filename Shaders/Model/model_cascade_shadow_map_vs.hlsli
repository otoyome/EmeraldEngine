#ifndef __MODEL_CSM_VS_HLSLI__
#define __MODEL_CSM_VS_HLSLI__

//#include "model.hlsli"

//	メッシュレット頂点情報読込
VS_SHADOW_MAP_IN load_vertex_pulled(MeshletContext meshlet_context, uint vertex_id)
{
    VS_SHADOW_MAP_IN vin = (VS_SHADOW_MAP_IN)0;
    uint global_vertex_index = meshlet_resolve_global_vertex_index(meshlet_context, vertex_id);

    vin.position = meshlet_load_position(global_vertex_index);
    vin.normal = meshlet_load_normal(global_vertex_index);
#if VERTEX_COLOR_MAX > 0
    vin.color[0] = meshlet_load_color0(global_vertex_index);
#endif  //  VERTEX_COLOR_MAX > 0
#if TEXCOORD_MAX > 0
    vin.texcoord[0] = meshlet_load_texcoord0(global_vertex_index);
#endif  //  TEXCOORD_MAX > 0
#if TEXCOORD_MAX > 1
    vin.texcoord[1] = meshlet_load_texcoord1(global_vertex_index);
#endif  //  TEXCOORD_MAX > 1
#if BONE_MAX > 0
    vin.joints[0] = meshlet_load_joints0(global_vertex_index);
    vin.weights[0] = meshlet_load_weights0(global_vertex_index);
#endif  //  BONE_MAX > 0
#if BONE_MAX > 4
    vin.joints[1] = meshlet_load_joints1(global_vertex_index);
    vin.weights[1] = meshlet_load_weights1(global_vertex_index);
#endif  //  BONE_MAX > 4
#if BONE_MAX > 8
    vin.joints[2] = meshlet_load_joints2(global_vertex_index);
    vin.weights[2] = meshlet_load_weights2(global_vertex_index);
#endif  //  BONE_MAX > 8
#if BONE_MAX > 12
    vin.joints[3] = meshlet_load_joints3(global_vertex_index);
    vin.weights[3] = meshlet_load_weights3(global_vertex_index);
#endif  //  BONE_MAX > 12

    return vin;
}

VS_SHADOW_MAP_OUT main(uint vertex_id : SV_VertexID)
{
    uint index;
    VS_SHADOW_MAP_OUT vout = (VS_SHADOW_MAP_OUT)0;
    MeshletContext meshlet_context = meshlet_build_context();
    VS_SHADOW_MAP_IN vin = load_vertex_pulled(meshlet_context, vertex_id);

#if defined(HAS_MORPH)  //  モーフィング対応
    uint global_vertex_index_for_morph = meshlet_resolve_global_vertex_index(meshlet_context, vertex_id);
    apply_morph_position(meshlet_context, vin.position, global_vertex_index_for_morph);
    apply_morph_normal(meshlet_context, vin.normal, global_vertex_index_for_morph);
#endif  //  defined(HAS_MORPH)

#if BONE_MAX > 0
    float3 position = 0, normal = 0;
    [unroll] for (index = 0; index < BoneMax; ++index)
    {
        uint bone_index = vin.joints[index / 4][index % 4];
        float bone_weight = vin.weights[index / 4][index % 4];
        position += mul(float4(vin.position, 1), get_bone_transform(meshlet_context, bone_index)).xyz * bone_weight;
        normal += mul(float4(vin.normal, 0), get_bone_transform(meshlet_context, bone_index)).xyz * bone_weight;
    }
#else  //  BONE_MAX > 0
    float3 position = vin.position;
    float3 normal = vin.normal;
#endif  //  BONE_MAX > 0

    vout.position = float4(position, 1.0f);
    vout.w_position.xyz = position;
    vout.w_position.w = 1;
    vout.w_normal.xyz = normal;
    vout.w_normal.w = 0;

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

#endif  //  __MODEL_CSM_VS_HLSLI__
