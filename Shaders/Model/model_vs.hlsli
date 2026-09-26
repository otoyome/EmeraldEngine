#ifndef __MODEL_VS_HLSLI__
#define __MODEL_VS_HLSLI__

//#include "model.hlsli"

#if defined(FORWARD_RENDERING)
#define VS_IN VS_FORWARD_IN
#define VS_OUT VS_FORWARD_OUT
#else // defined(FORWARD_RENDERING)
#define VS_IN VS_GBUFFER_IN
#define VS_OUT VS_GBUFFER_OUT
#endif // defined(FORWARD_RENDERING)

//	メッシュレット頂点情報読込
VS_IN load_vertex_pulled(uint vertex_id)
{
    VS_IN vin = (VS_IN)0;
    uint global_vertex_index = meshlet_resolve_global_vertex_index(vertex_id);

    vin.position = meshlet_load_position(global_vertex_index);
    vin.normal = meshlet_load_normal(global_vertex_index);
#if defined(HAS_TANGENT)
    vin.tangent = meshlet_load_tangent(global_vertex_index);
#endif  //  defined(HAS_TANGENT)
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

VS_OUT main(uint vertex_id : SV_VertexID)
{
    uint index;
    VS_OUT vout = (VS_OUT) 0;
    VS_IN vin = load_vertex_pulled(vertex_id);

#if defined(HAS_MORPH)  //  モーフィング対応
    uint global_vertex_index_for_morph = meshlet_resolve_global_vertex_index(vertex_id);
    apply_morph_position(vin.position, global_vertex_index_for_morph);
    apply_morph_normal(vin.normal, global_vertex_index_for_morph);
#endif  //  defined(HAS_MORPH)

#if BONE_MAX > 0

    float3 current_position = 0, normal = 0, previous_position = 0;
#if defined(HAS_TANGENT)
    float3 tangent = 0;
#endif  //  defined(HAS_TANGENT)
    [unroll] for (index = 0; index < BoneMax; ++index)
    {
        uint row = index / 4u;
        uint col = index % 4u;
        uint bone_index = vin.joints[row][col];
        float bone_weight = vin.weights[row][col];
        current_position += mul(float4(vin.position, 1), get_bone_transform(bone_index)).xyz * bone_weight;
        normal += mul(float4(vin.normal, 0), get_bone_transform(bone_index)).xyz * bone_weight;
#if defined(HAS_TANGENT)
        tangent += mul(float4(vin.tangent.xyz, 0), get_bone_transform(bone_index)).xyz * bone_weight;
#endif  //  defined(HAS_TANGENT)

        previous_position += mul(float4(vin.position, 1), get_previous_bone_transform(bone_index)).xyz * bone_weight;
    }

#else  //  BONE_MAX > 0

    float3 current_position = vin.position.xyz;
    float3 normal = vin.normal.xyz;
    float3 previous_position = vin.position.xyz;
#if defined(HAS_TANGENT)
    float3 tangent = vin.tangent.xyz;
#endif  //  defined(HAS_TANGENT)

#endif  //  BONE_MAX > 0

    vout.w_position.xyz = current_position;
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

    vout.position = mul(float4(vout.w_position.xyz, 1), view_projection_transform);
    vout.current_clip_position = vout.position;
    vout.previous_clip_position = mul(float4(previous_position, 1), previous_view_projection_transform);
#if defined(FORWARD_RENDERING)
    [unroll] for (index = 0; index < shadow_map_max; ++index)
    {
        if (index >= shadow_map_count)
            break;

        vout.shadow_texcoords[index] = mul(float4(vout.w_position.xyz, 1.0f), light_view_projection_transform[index]);
    }
#endif // defined(FORWARD_RENDERING)

    return vout;
}

#endif  //  __MODEL_VS_HLSLI__
