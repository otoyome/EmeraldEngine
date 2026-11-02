#ifndef __MODEL_CSM_VS_HLSLI__
#define __MODEL_CSM_VS_HLSLI__

//#include "model.hlsli"

//	@tool 境界球がビュー(カスケード)の視錐台の外にあるか
//	@tool 左右上下の4平面だけで判定する(奥行き方向は視錐台の外にある物も影を落とすので判定しない)
bool shadow_meshlet_is_outside(float3 center, float radius, float4x4 view_projection)
{
    float4 column_x = float4(view_projection[0].x, view_projection[1].x, view_projection[2].x, view_projection[3].x);
    float4 column_y = float4(view_projection[0].y, view_projection[1].y, view_projection[2].y, view_projection[3].y);
    float4 column_w = float4(view_projection[0].w, view_projection[1].w, view_projection[2].w, view_projection[3].w);
    float4 planes[4] = { column_w + column_x, column_w - column_x, column_w + column_y, column_w - column_y };

    [unroll] for (uint index = 0; index < 4; ++index)
    {
        float4 plane = planes[index];
        float distance = (dot(plane.xyz, center) + plane.w) / length(plane.xyz);
        if (distance < -radius)
            return true;
    }
    return false;
}

VS_SHADOW_MAP_OUT main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    uint index;
    VS_SHADOW_MAP_OUT vout = (VS_SHADOW_MAP_OUT)0;

    //	@tool インスタンス番号をメッシュレットとカスケードに分ける
    uint record_index, cascade_index;
    meshlet_decode_instance(instance_id, shadow_map_count, record_index, cascade_index);
    vout.render_target_array_index = cascade_index;

    //	@tool カスケードの範囲外のメッシュレットは、頂点を読む前に縮退ポリゴン化する
    float4x4 view_projection = light_view_projection_transform[cascade_index];
    MeshletInstanceRecord record = visible_meshlet_instances[record_index];
    if (record.no_cull == 0 && record.bounds_radius > 0.0f && shadow_meshlet_is_outside(record.bounds_center, record.bounds_radius, view_projection))
    {
        vout.position = float4(0, 0, 0, 0);
        return vout;
    }

	//	メッシュレット構築
    bool is_degenerate;
    MeshletContext meshlet_context = meshlet_build_context(record_index, vertex_id, is_degenerate);
    if (is_degenerate)
    {
        //  構築できない場合は縮退ポリゴン化
        vout.position = float4(0, 0, 0, 0);
        return vout;
    }

    //	頂点読込・モーフ・スキニング
    VS_MODEL_IN vin = load_model_vertex(meshlet_context, vertex_id);
    ModelVertex vertex = skin_model_vertex(meshlet_context, vin);

    vout.position = mul(float4(vertex.position, 1.0f), view_projection);
    vout.w_position.xyz = vertex.position;
    vout.w_position.w = 1;
    vout.w_normal.xyz = vertex.normal;
    vout.w_normal.w = 0;

    //	深度補正
    {
        float4 offset_parameter = shadow_bias[cascade_index];
        float NoL = abs(dot(float3(view_projection[0].z, view_projection[1].z, view_projection[2].z), vertex.normal));
        float slope = clamp(abs(NoL) > 0 ? sqrt(saturate(1.0f - NoL * NoL)) / NoL
                                         : offset_parameter.z, 0.0f, offset_parameter.z );
        float slope_bias = offset_parameter.y * slope;
        float depth_bias = offset_parameter.x + slope_bias;
        vout.position.z -= depth_bias;	//	Reversed-Z: 値が小さいほど遠いので引き算に
    }

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
