#ifndef __DEFAULT_DECAL_DATA_PS_HLSLI__
#define __DEFAULT_DECAL_DATA_PS_HLSLI__

DecalData decal_ps(DecalIn din)
{
    DecalData dout = (DecalData) 0;

    //  ベースカラー適応
    dout.base_color = base_color_factor;
    if (HAS_RESOURCE(BaseColorMap))
    {
        dout.base_color *= TEXTURE2D_SAMPLE(BaseColorMap, GET_SAMPLER_STATE_INDEX(BaseColorMap), din.texcoord);
    }

    //  法線マップ適応
    dout.w_normal = din.w_normal;
    if (HAS_RESOURCE(NormalMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(NormalMap, GET_SAMPLER_STATE_INDEX(NormalMap), din.texcoord);
        float3 normal_factor = (sampled.xyz * 2.0f) - 1.0f;
        normal_factor = normalize(normal_factor * float3(normal_map_scale, normal_map_scale, 1.0f));
        dout.w_normal = normalize((normal_factor.x * din.w_tangent)
                                + (normal_factor.y * din.w_binormal)
                                + (normal_factor.z * din.w_normal));
    }

    //  深度出力
    dout.linear_depth = din.linear_depth;
    
    return dout;
}

#endif  //  __DEFAULT_DECAL_DATA_PS_HLSLI__
