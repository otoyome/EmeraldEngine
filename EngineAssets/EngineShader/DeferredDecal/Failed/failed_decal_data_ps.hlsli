#ifndef __FAILED_DECAL_DATA_PS_HLSLI__
#define __FAILED_DECAL_DATA_PS_HLSLI__

DecalData decal_ps(DecalIn din)
{
    DecalData dout = (DecalData) 0;

    //  ベースカラー適応
    dout.base_color = float4(0.8, 0.3, 0.7, 1.0f);
    dout.w_normal = din.w_normal;
    dout.linear_depth = din.linear_depth;
    return dout;
}

#endif  //  __FAILED_DECAL_DATA_PS_HLSLI__
