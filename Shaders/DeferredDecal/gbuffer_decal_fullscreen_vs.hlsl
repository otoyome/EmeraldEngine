#include "gbuffer_decal.hlsli"

VS_OUT main(uint vin : SV_VERTEXID)
{
    VS_OUT vout;
    vout.position.xy = float2(vin % 2, vin / 2) * float2(2.0f, -2.0f) - float2(1.0f, -1.0f);
    vout.position.z = depth_near_value;
    vout.position.w = 1;
    return vout;
}
