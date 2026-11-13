#ifndef __COMMON_GBUFFER_WRITER_HLSLI__
#define __COMMON_GBUFFER_WRITER_HLSLI__

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PS_GBUFFER_OUT
{
    float4 base_color : SV_TARGET0;
    float4 normal_tangent : SV_TARGET1;
    float4 parameter : SV_TARGET2;
    float2 velocity : SV_TARGET3;
    float4 custom_data0 : SV_TARGET4;
    float4 custom_data1 : SV_TARGET5;
    float4 custom_data2 : SV_TARGET6;
    
    float4 scene_color : SV_TARGET7; //  é©å»î≠åıèëÇ´çûÇ›óp
};

#endif  //  __COMMON_GBUFFER_WRITER_HLSLI__