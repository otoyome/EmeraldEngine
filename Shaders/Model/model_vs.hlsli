#ifndef __MODEL_VS_HLSLI__
#define __MODEL_VS_HLSLI__

//#include "model.hlsli"

#if defined(FORWARD_RENDERING)
#define VS_OUT VS_FORWARD_OUT
#else // defined(FORWARD_RENDERING)
#define VS_OUT VS_GBUFFER_OUT
#endif // defined(FORWARD_RENDERING)

VS_OUT main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    uint index;
    VS_OUT vout = (VS_OUT) 0;

	//	メッシュレット構築
    bool is_degenerate;
    MeshletContext meshlet_context = meshlet_build_context(instance_id, vertex_id, is_degenerate);
    if (is_degenerate)
    {
        //  構築できない場合は縮退ポリゴン化
        vout.position = float4(0, 0, 0, 0);
        return vout;
    }

    //	ピクセルシェーダー側でもprimitive_indexが欲しいときがあるので
    vout.primitive_index = meshlet_context.primitive_index;

#if !defined(FORWARD_RENDERING)
	vout.meshlet_debug_id = meshlet_context.primitive_index * 1000003u + meshlet_context.meshlet_triangle_offset;
#endif	//	!defined(FORWARD_RENDERING)

    //	頂点読込・モーフ・スキニング
    VS_MODEL_IN vin = load_model_vertex(meshlet_context, vertex_id);
    ModelVertex vertex = skin_model_vertex(meshlet_context, vin);
    float3 previous_position = skin_previous_position(meshlet_context, vin);

    vout.w_position.xyz = vertex.position;
    vout.w_position.w = 1.0f;
    vout.w_normal.xyz = normalize(vertex.normal).xyz;
    vout.w_normal.w = 0.0f;

#if defined(HAS_TANGENT)

    float3 tangent = vertex.tangent;
	//	法線に対して接線を再直交化
    tangent = tangent - dot(tangent, vout.w_normal.xyz) * vout.w_normal.xyz;

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
