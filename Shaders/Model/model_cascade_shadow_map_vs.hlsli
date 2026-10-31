#ifndef __MODEL_CSM_VS_HLSLI__
#define __MODEL_CSM_VS_HLSLI__

//#include "model.hlsli"

VS_SHADOW_MAP_OUT main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    uint index;
    VS_SHADOW_MAP_OUT vout = (VS_SHADOW_MAP_OUT)0;

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

    vout.position = float4(vertex.position, 1.0f);
    vout.w_position.xyz = vertex.position;
    vout.w_position.w = 1;
    vout.w_normal.xyz = vertex.normal;
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
