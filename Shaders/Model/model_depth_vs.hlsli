#ifndef __MODEL_DEPTH_VS_HLSLI__
#define __MODEL_DEPTH_VS_HLSLI__

//#include "model.hlsli"

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

    //	頂点読込・モーフ・スキニング
    VS_MODEL_IN vin = load_model_vertex(meshlet_context, vertex_id);
    ModelVertex vertex = skin_model_vertex(meshlet_context, vin);

    vout.position = mul(float4(vertex.position, 1), view_projection_transform);
    vout.w_position.xyz = vertex.position;
    vout.w_position.w = 1.0f;
    vout.w_normal.xyz = normalize(vertex.normal).xyz;
    vout.w_normal.w = 0.0f;
#if defined(HAS_TANGENT)
    vout.w_tangent.xyz = normalize(vertex.tangent).xyz;
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
